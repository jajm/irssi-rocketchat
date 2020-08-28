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
#include "libwebsockets.h"
#include "jansson.h"

#define STRUCT_SERVER_REC SERVER_REC
typedef struct _ROCKETCHAT_QUERY_REC {
	#include "query-rec.h"
} ROCKETCHAT_QUERY_REC;

struct rocketchat_per_session_data {
	struct lws_ring *ring;
	GString *buffer;
};

static CHATNET_REC *
rocketchat_create_chatnet(void)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return g_new0(CHATNET_REC, 1);
}

static SERVER_SETUP_REC *
rocketchat_create_server_setup(void)
{

	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return g_new0(SERVER_SETUP_REC, 1);
}

static SERVER_CONNECT_REC *
rocketchat_create_server_connect(void)
{
	SERVER_CONNECT_REC *conn;
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);

	conn = g_new0(SERVER_CONNECT_REC, 1);
	return conn;
}

static CHANNEL_SETUP_REC *
rocketchat_create_channel_setup(void)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return g_new0(CHANNEL_SETUP_REC, 1);
}

static void
rocketchat_destroy_server_connect(SERVER_CONNECT_REC *conn)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
}

static void
rocketchat_channels_join(SERVER_REC *server, const char *data, int automatic)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
}

static int
rocketchat_isnickflag(SERVER_REC *server, char flag)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return 0;
}

static int rocketchat_ischannel(SERVER_REC *server, const char *data)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return 0;
}

static const char *
rocketchat_get_nick_flags(SERVER_REC *server)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return "";
}

static void
rocketchat_send_message(SERVER_REC *server, const char *target, const char *msg, int target_type)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
}

static SERVER_REC *
rocketchat_server_init_connect(SERVER_CONNECT_REC *connrec)
{
	SERVER_REC *server;

	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	printtext(NULL, NULL, MSGLEVEL_MSGS, connrec->address);

	server = g_new0(SERVER_REC, 1);
	server->chat_type = chat_protocol_lookup("rocketchat");
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

	server_connect_init(server);

	return server;
}

static int callback_minimal(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	SERVER_REC *server;
	json_t *json, *json_msg;
	json_error_t json_error;
	struct rocketchat_per_session_data *pss = user;

	server = lws_get_opaque_user_data(wsi);

	lwsl_user("reason: %d", reason);

	if (server && server->disconnected) {
		printtext(NULL, NULL, MSGLEVEL_MSGS, "disconnected");
		return -1;
	}

	switch (reason) {
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			if (server && in) {
				server_connect_failed(server, in);
			}
			return -1;

		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			printtext(NULL, NULL, MSGLEVEL_MSGS, in);
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			pss->ring = lws_ring_create(sizeof(json_t *), 128, NULL);
			pss->buffer = g_string_new(NULL);
			printtext(NULL, NULL, MSGLEVEL_MSGS, "established");
			server_connect_finished(server);
			json = json_object();
			json_object_set_new(json, "msg", json_string("connect"));
			json_object_set_new(json, "version", json_string("1"));
			json_object_set_new(json, "support", json_array());
			json_array_append_new(json_object_get(json, "support"), json_string("1"));
			lws_ring_insert(pss->ring, &json, 1);
			lws_callback_on_writable(wsi);
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			lws_ring_destroy(pss->ring);
			g_string_free(pss->buffer, TRUE);
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			if (1 == lws_ring_consume(pss->ring, NULL, &json, 1)) {
				size_t size = json_dumpb(json, NULL, 0, 0);
				if (size != 0) {
					char *buffer = malloc(LWS_PRE + size);
					json_dumpb(json, buffer + LWS_PRE, size, 0);
					lwsl_user("T: %*s", (int)size, buffer + LWS_PRE);
					lws_write(wsi, (unsigned char *)buffer + LWS_PRE, size, LWS_WRITE_TEXT);
					free(buffer);
				}
				json_decref(json);
			}
			if (0 < lws_ring_get_count_waiting_elements(pss->ring, NULL)) {
				lws_callback_on_writable(wsi);
			}
			break;

		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			const size_t remaining = lws_remaining_packet_payload(wsi);
			if (!remaining && lws_is_final_fragment(wsi)) {
				g_string_append_len(pss->buffer, in, len);

				//printtext(NULL, NULL, MSGLEVEL_MSGS, pss->buffer->str);
				lwsl_user("R: %s", (char *)pss->buffer->str);
				json = json_loadb(pss->buffer->str, pss->buffer->len, 0, &json_error);
				g_string_truncate(pss->buffer, 0);
				if (!json) {
					lwsl_err("json error: %s", json_error.text);
					break;
				}

				json_msg = json_object_get(json, "msg");
				if (json_msg) {
					const char *msg = json_string_value(json_msg);
					if (!strcmp("ping", msg)) {
						json_t *message = json_object();
						json_object_set_new(message, "msg", json_string("pong"));
						lws_ring_insert(pss->ring, &message, 1);
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

						lws_ring_insert(pss->ring, &message, 1);
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

								lws_ring_insert(pss->ring, &message, 1);
								lws_callback_on_writable(wsi);
							}
						} else if (!strcmp(id, "rooms/get")) {
							json_t *result = json_object_get(json, "result");
							json_t *update = json_object_get(result, "update");
							json_t *value;
							size_t index;
							json_array_foreach(update, index, value) {
								const char *rid = json_string_value(json_object_get(value, "_id"));
								json_t *params = json_array();
								json_array_append_new(params, json_string(rid));
								json_array_append_new(params, json_false());
								json_t *message = json_object();
								json_object_set_new(message, "msg", json_string("sub"));
								json_object_set_new(message, "id", json_sprintf("sub-stream-room-messages-%s", rid));
								json_object_set_new(message, "name", json_string("stream-room-messages"));
								json_object_set_new(message, "params", params);
								lws_ring_insert(pss->ring, &message, 1);
							}
							lws_callback_on_writable(wsi);
						}
					}
				}
				json_decref(json);
			} else {
				g_string_append_len(pss->buffer, in, len);
			}
		}
		break;

		default:
			break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

static const struct lws_protocols protocols[] = {
	{ "dumb-increment-protocol", callback_minimal, sizeof(struct rocketchat_per_session_data), 1024, 0, NULL, 0 },
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

static void rocketchat_lws_log_emit (int level, const char *line)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, line);
}

static void
rocketchat_server_connect(SERVER_REC *server)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);

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
	client_connect_info.ssl_connection = LCCSCF_USE_SSL;
	client_connect_info.protocol = protocols[0].name; /* "dumb-increment-protocol" */
	client_connect_info.opaque_user_data = server;

	lws_client_connect_via_info(&client_connect_info);
}

static CHANNEL_REC *
rocketchat_channel_create(SERVER_REC *server, const char *name, const char *visible_name,
    int automatic)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	return g_new0(CHANNEL_REC, 1);
}

static QUERY_REC *
rocketchat_query_create(const char *server_tag, const char *data, int automatic)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	ROCKETCHAT_QUERY_REC *query_rec;

	query_rec = g_new0(ROCKETCHAT_QUERY_REC, 1);
	query_rec->chat_type = chat_protocol_lookup("rocketchat");
	query_rec->name = g_strdup(data);
	query_init((QUERY_REC*)query_rec, automatic);

	return (QUERY_REC*)query_rec;
}

void rocketchat_core_init(void)
{
	CHAT_PROTOCOL_REC *chat_protocol;

	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);

	chat_protocol = g_new0(CHAT_PROTOCOL_REC, 1);
	chat_protocol->name = "rocketchat";
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

	module_register("rocketchat", "core");
}

void rocketchat_core_deinit(void)
{
	printtext(NULL, NULL, MSGLEVEL_MSGS, __func__);
	signal_emit("chat protocol deinit", 1, chat_protocol_find("rocketchat"));
	chat_protocol_unregister("rocketchat");
}

#ifdef IRSSI_ABI_VERSION
void rocketchat_core_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
