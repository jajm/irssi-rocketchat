#include "module.h"
#include "chat-protocols.h"
#include "chatnets.h"
#include "servers-setup.h"
#include "servers.h"
#include "channels.h"
#include "channels-setup.h"
#include "signals.h"
#include "queries.h"
#include "levels.h"
#include "printtext.h"
#include "rocketchat-servers.h"
#include "rocketchat-queries.h"
#include "libwebsockets.h"
#include "jansson.h"

static CHATNET_REC *
rocketchat_create_chatnet(void)
{
	return g_new0(CHATNET_REC, 1);
}

static SERVER_SETUP_REC *
rocketchat_create_server_setup(void)
{

	return g_new0(SERVER_SETUP_REC, 1);
}

static SERVER_CONNECT_REC *
rocketchat_create_server_connect(void)
{
	SERVER_CONNECT_REC *conn;

	conn = g_new0(SERVER_CONNECT_REC, 1);
	return conn;
}

static CHANNEL_SETUP_REC *
rocketchat_create_channel_setup(void)
{
	return g_new0(CHANNEL_SETUP_REC, 1);
}

static void
rocketchat_destroy_server_connect(SERVER_CONNECT_REC *conn)
{
}

static void
rocketchat_channels_join(SERVER_REC *server, const char *data, int automatic)
{
}

static int
rocketchat_isnickflag(SERVER_REC *server, char flag)
{
	return 0;
}

static int rocketchat_ischannel(SERVER_REC *server, const char *data)
{
	return 0;
}

static const char *
rocketchat_get_nick_flags(SERVER_REC *server)
{
	return "";
}

static void
rocketchat_send_message(SERVER_REC *server_rec, const char *target, const char *msg, int target_type)
{
	ROCKETCHAT_SERVER_REC *server = (ROCKETCHAT_SERVER_REC *)server_rec;

	json_t *message = json_object();
	json_object_set_new(message, "rid", json_string(target));
	json_object_set_new(message, "msg", json_string(msg));

	json_t *params = json_array();
	json_array_append_new(params, message);

	json_t *root = json_object();
	json_object_set_new(root, "msg", json_string("method"));
	json_object_set_new(root, "method", json_string("sendMessage"));
	json_object_set_new(root, "id", json_sprintf("%s-%s", "sendMessage", target));
	json_object_set_new(root, "params", params);

	g_queue_push_tail(server->message_queue, root);
	lws_callback_on_writable(server->wsi);
}

static SERVER_REC *
rocketchat_server_init_connect(SERVER_CONNECT_REC *connrec)
{
	ROCKETCHAT_SERVER_REC *server;

	server = g_new0(ROCKETCHAT_SERVER_REC, 1);
	server->chat_type = ROCKETCHAT_PROTOCOL;
	server->connrec = connrec;
	server->channels_join = rocketchat_channels_join;
	server->isnickflag = rocketchat_isnickflag;
	server->ischannel = rocketchat_ischannel;
	server->get_nick_flags = rocketchat_get_nick_flags;
	server->send_message = rocketchat_send_message;
	server->disconnected = FALSE;

	server_connect_ref(server->connrec);

	// do not use irssi socket
	server->connrec->no_connect = TRUE;
	server->connect_pid = -1;

	server_connect_init((SERVER_REC *)server);

	return (SERVER_REC *)server;
}

static int rocketchat_lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	ROCKETCHAT_SERVER_REC *server;
	json_t *json, *json_msg;
	json_error_t json_error;

	// printtext(NULL, NULL, MSGLEVEL_CRAP, "reason: %d", reason);

	if (reason == LWS_CALLBACK_PROTOCOL_INIT) {
		return 0;
	}

	server = lws_get_opaque_user_data(wsi);

	if (!server || server->disconnected) {
		printtext(server, NULL, MSGLEVEL_CRAP, "disconnected");
		return -1;
	}

	switch (reason) {
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			server_connect_failed((SERVER_REC *)server, in);
			return -1;

		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			printtext(server, NULL, MSGLEVEL_CRAP, "peer initiated close");
			server_disconnect((SERVER_REC *)server);
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lookup_servers = g_slist_remove(lookup_servers, server);
			server->connected = TRUE;
			server->wsi = wsi;
			server->message_queue = g_queue_new();
			server->buffer = g_string_new(NULL);
			server_connect_finished((SERVER_REC *)server);

			json = json_object();
			json_object_set_new(json, "msg", json_string("connect"));
			json_object_set_new(json, "version", json_string("1"));
			json_object_set_new(json, "support", json_array());
			json_array_append_new(json_object_get(json, "support"), json_string("1"));
			g_queue_push_tail(server->message_queue, json);
			lws_callback_on_writable(wsi);
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			if (server) {
				server_disconnect((SERVER_REC *)server);
			}
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			json = g_queue_pop_head(server->message_queue);
			if (json) {
				size_t size = json_dumpb(json, NULL, 0, 0);
				if (size != 0) {
					char *buffer = malloc(LWS_PRE + size + 1);
					json_dumpb(json, buffer + LWS_PRE, size, 0);
					buffer[LWS_PRE + size] = '\0';
					printtext(server, NULL, MSGLEVEL_CRAP, "T: %s", buffer + LWS_PRE);
					lws_write(wsi, (unsigned char *)buffer + LWS_PRE, size, LWS_WRITE_TEXT);
					free(buffer);
				}
				json_decref(json);
			}
			if (!g_queue_is_empty(server->message_queue)) {
				lws_callback_on_writable(wsi);
			}
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			const size_t remaining = lws_remaining_packet_payload(wsi);
			if (!remaining && lws_is_final_fragment(wsi)) {
				g_string_append_len(server->buffer, in, len);

				printtext(server, NULL, MSGLEVEL_CRAP, "R: %s", server->buffer->str);
				json = json_loadb(server->buffer->str, server->buffer->len, 0, &json_error);
				g_string_truncate(server->buffer, 0);
				if (!json) {
					printtext(server, NULL, MSGLEVEL_CRAP, "json error: %s", json_error.text);
					break;
				}

				json_msg = json_object_get(json, "msg");
				if (json_msg) {
					const char *msg = json_string_value(json_msg);
					if (!strcmp("ping", msg)) {
						json_t *message = json_object();
						json_object_set_new(message, "msg", json_string("pong"));
						g_queue_push_tail(server->message_queue, message);
						lws_callback_on_writable(wsi);
					} else if (!strcmp("connected", msg)) {
						json_t *credential = json_object();
						json_object_set_new(credential, "resume", json_string(server->connrec->password));

						json_t *params = json_array();
						json_array_append_new(params, credential);

						json_t *message = json_object();
						json_object_set_new(message, "msg", json_string("method"));
						json_object_set_new(message, "method", json_string("login"));
						json_object_set_new(message, "id", json_string("login"));
						json_object_set_new(message, "params", params);

						g_queue_push_tail(server->message_queue, message);
						lws_callback_on_writable(wsi);
					} else if (!strcmp("result", msg)) {
						const char *id = json_string_value(json_object_get(json, "id"));
						if (!strcmp(id, "login")) {
							json_t *result = json_object_get(json, "result");
							if (result) {
								// Login successful !
								json_t *param = json_object();
								json_object_set_new(param, "$date", json_integer(0));

								json_t *params = json_array();
								json_array_append_new(params, param);

								json_t *message = json_object();
								json_object_set_new(message, "msg", json_string("method"));
								json_object_set_new(message, "method", json_string("rooms/get"));
								json_object_set_new(message, "id", json_string("rooms/get"));
								json_object_set_new(message, "params", params);

								g_queue_push_tail(server->message_queue, message);
								lws_callback_on_writable(wsi);
							}
						} else if (!strcmp(id, "rooms/get")) {
							json_t *result = json_object_get(json, "result");
							json_t *update = json_object_get(result, "update");
							json_t *value;
							size_t index;
							json_array_foreach(update, index, value) {
								const char *rid = json_string_value(json_object_get(value, "_id"));
								const char *name = json_string_value(json_object_get(value, "name"));
								const char *fname = json_string_value(json_object_get(value, "fname"));
								char *room_name;
								if (fname) {
									room_name = g_strjoin("#", fname, NULL);
								} else if (name) {
									room_name = g_strjoin("#", name, NULL);
								} else {
									json_t *usernames = json_object_get(value, "usernames");
									gchar **usernames_str = g_new0(gchar *, json_array_size(usernames));
									size_t i, j = 0;
									json_t *username;
									json_array_foreach(usernames, i, username) {
										if (strcmp(json_string_value(username), server->nick)) {
											usernames_str[j] = json_string_value(username);
											j++;
										}
									}
									room_name = g_strjoinv(",", usernames_str);
								}
								CHANNEL_REC *channel = g_new0(CHANNEL_REC, 1);
								channel->chat_type = ROCKETCHAT_PROTOCOL;
								channel_init(channel, (SERVER_REC *)server, rid, room_name, TRUE);
								g_free(room_name);

								json_t *params = json_array();
								json_array_append_new(params, json_string(rid));
								json_array_append_new(params, json_false());
								json_t *message = json_object();
								json_object_set_new(message, "msg", json_string("sub"));
								json_object_set_new(message, "id", json_sprintf("sub-stream-room-messages-%s", rid));
								json_object_set_new(message, "name", json_string("stream-room-messages"));
								json_object_set_new(message, "params", params);
								g_queue_push_tail(server->message_queue, message);
							}
							lws_callback_on_writable(wsi);
						}
					} else if (!strcmp(msg, "added")) {
						const char *collection = json_string_value(json_object_get(json, "collection"));
						if (!strcmp(collection, "users")) {
							json_t *fields = json_object_get(json, "fields");
							g_free(server->nick);
							server->nick = g_strdup(json_string_value(json_object_get(fields, "username")));
						}
					} else if (!strcmp(msg, "changed")) {
						const char *collection = json_string_value(json_object_get(json, "collection"));
						if (!strcmp(collection, "stream-room-messages")) {
							json_t *fields = json_object_get(json, "fields");
							json_t *args = json_object_get(fields, "args");
							json_t *message = json_array_get(args, 0);
							json_t *replies = json_object_get(message, "replies");
							json_t *reactions = json_object_get(message, "reactions");
							json_t *editedAt = json_object_get(message, "editedAt");
							json_t *t = json_object_get(message, "t");
							if (!replies && !reactions && !editedAt && !t) {
								const char *nick = json_string_value(json_object_get(json_object_get(message, "u"), "username"));
								const char *rid = json_string_value(json_object_get(message, "rid"));
								signal_emit("message public", 5, server, json_string_value(json_object_get(message, "msg")), nick, NULL, rid);
							}
						}
					}
				}
				json_decref(json);
			} else {
				g_string_append_len(server->buffer, in, len);
			}
		}
		break;

		default:
			break;
	}

	return 0;
}

static const struct lws_protocols protocols[] = {
	{ "rocketchat", rocketchat_lws_callback, 0, 0, 0, NULL, 0 },
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

static void rocketchat_lws_log_emit (int level, const char *line)
{
	printtext(NULL, NULL, MSGLEVEL_CRAP, line);
}

static void
rocketchat_server_connect(SERVER_REC *server)
{
	struct lws_context_creation_info context_creation_info;
	struct lws_client_connect_info client_connect_info;
	struct lws_context *context;
	void *foreign_loops[1];
	GMainLoop *ml;

	ml = g_main_loop_new(NULL, TRUE);

	lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE, rocketchat_lws_log_emit);

	memset(&context_creation_info, 0, sizeof context_creation_info);
	context_creation_info.port = CONTEXT_PORT_NO_LISTEN;
	context_creation_info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_GLIB;
	context_creation_info.protocols = protocols;
	foreign_loops[0] = (void *)ml;
	context_creation_info.foreign_loops = foreign_loops;

	// TCP keepalive
	context_creation_info.ka_time = 60;
	context_creation_info.ka_probes = 3;
	context_creation_info.ka_interval = 5;

	context = lws_create_context(&context_creation_info);
	if (!context) {
		server_connect_failed(server, "lws init failed");
		return;
	}

	memset(&client_connect_info, 0, sizeof client_connect_info);
	client_connect_info.context = context;
	client_connect_info.port = server->connrec->port;
	client_connect_info.address = server->connrec->address;
	client_connect_info.path = "/websocket";
	client_connect_info.host = client_connect_info.address;
	client_connect_info.origin = client_connect_info.address;
	if (server->connrec->use_tls) {
		client_connect_info.ssl_connection = LCCSCF_USE_SSL;
	}
	client_connect_info.opaque_user_data = server;

	lws_client_connect_via_info(&client_connect_info);
}

static CHANNEL_REC *
rocketchat_channel_create(SERVER_REC *server, const char *name, const char *visible_name,
    int automatic)
{
	return g_new0(CHANNEL_REC, 1);
}

static QUERY_REC *
rocketchat_query_create(const char *server_tag, const char *data, int automatic)
{
	ROCKETCHAT_QUERY_REC *query_rec;

	query_rec = g_new0(ROCKETCHAT_QUERY_REC, 1);
	query_rec->chat_type = ROCKETCHAT_PROTOCOL;
	query_rec->name = g_strdup(data);
	query_init((QUERY_REC*)query_rec, automatic);

	return (QUERY_REC*)query_rec;
}

void rocketchat_core_init(void)
{
	CHAT_PROTOCOL_REC *chat_protocol;

	chat_protocol = g_new0(CHAT_PROTOCOL_REC, 1);
	chat_protocol->name = ROCKETCHAT_PROTOCOL_NAME;
	chat_protocol->fullname = "Rocket.Chat";
	chat_protocol->chatnet = "rocketchat";
	chat_protocol->case_insensitive = FALSE;
	chat_protocol->create_chatnet = rocketchat_create_chatnet;
	chat_protocol->create_server_setup = rocketchat_create_server_setup;
	chat_protocol->create_server_connect = rocketchat_create_server_connect;
	chat_protocol->create_channel_setup = rocketchat_create_channel_setup;
	chat_protocol->destroy_server_connect = rocketchat_destroy_server_connect;
	chat_protocol->server_init_connect = rocketchat_server_init_connect;
	chat_protocol->server_connect = rocketchat_server_connect;
	chat_protocol->channel_create = rocketchat_channel_create;
	chat_protocol->query_create = rocketchat_query_create;

	chat_protocol_register(chat_protocol);
	g_free(chat_protocol);

	rocketchat_servers_init();

	module_register("rocketchat", "core");
}

void rocketchat_core_deinit(void)
{
	rocketchat_servers_deinit();

	signal_emit("chat protocol deinit", 1, chat_protocol_find(ROCKETCHAT_PROTOCOL_NAME));
	chat_protocol_unregister(ROCKETCHAT_PROTOCOL_NAME);
}

#ifdef IRSSI_ABI_VERSION
void rocketchat_core_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
