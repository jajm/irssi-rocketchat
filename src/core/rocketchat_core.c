#include "module.h"
#include "chat-protocols.h"
#include "chatnets.h"
#include "servers-setup.h"
#include "servers.h"
#include "channels.h"
#include "channels-setup.h"
#include "signals.h"
#include "queries.h"
#include "commands.h"
#include "levels.h"
#include "printtext.h"
#include "nicklist.h"
#include "rocketchat-servers.h"
#include "rocketchat-servers-reconnect.h"
#include "rocketchat-queries.h"
#include "rocketchat-commands.h"
#include "rocketchat-result-callbacks.h"
#include "rocketchat-protocol.h"
#include "libwebsockets.h"
#include "jansson.h"

static void result_cb_sendMessage(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result;
	const char *message_id;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	message_id = json_string_value(json_object_get(result, "_id"));
	g_hash_table_insert(server->sent_messages, g_strdup(message_id), NULL);
}

static void result_cb_get_subscriptions_after_login(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *result, *subscription;
	size_t index;

	result = json_object_get(json, "result");
	json_array_foreach(result, index, subscription) {
		const char *rid = json_string_value(json_object_get(subscription, "rid"));

		rocketchat_subscribe(server, "stream-room-messages", rid);
	}
}

static void result_cb_login(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	GSList *tmp;
	GString *chans;
	json_t *error, *result;
	const char *userId;
	char *event;

	error = json_object_get(json, "error");
	if (error != NULL) {
		server_disconnect((SERVER_REC *)server);
		return;
	}

	result = json_object_get(json, "result");
	userId = json_string_value(json_object_get(result, "id"));

	g_free(server->userId);
	server->userId = g_strdup(userId);

	callback = rocketchat_result_callback_new(result_cb_get_subscriptions_after_login, NULL);
	rocketchat_call(server, "subscriptions/get", json_array(), callback);

	event = g_strconcat(server->userId, "/message", NULL);
	rocketchat_subscribe(server, "stream-notify-user", event);
	g_free(event);

	event = g_strconcat(server->userId, "/subscriptions-changed", NULL);
	rocketchat_subscribe(server, "stream-notify-user", event);
	g_free(event);

	event = g_strconcat(server->userId, "/notification", NULL);
	rocketchat_subscribe(server, "stream-notify-user", event);
	g_free(event);

	event = g_strconcat(server->userId, "/rooms-changed", NULL);
	rocketchat_subscribe(server, "stream-notify-user", event);
	g_free(event);

	event = g_strconcat(server->userId, "/userData", NULL);
	rocketchat_subscribe(server, "stream-notify-user", event);
	g_free(event);

	rocketchat_subscribe(server, "stream-notify-logged", "user-status");

	/* join to the channels marked with autojoin in setup */
	chans = g_string_new(NULL);
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		CHANNEL_SETUP_REC *rec = tmp->data;

		if (!rec->autojoin ||
		    !channel_chatnet_match(rec->chatnet,
					   server->connrec->chatnet))
			continue;

		/* check that we haven't already joined this channel in
		   same chat network connection.. */
                if (channel_find((SERVER_REC *)server, rec->name) == NULL)
			g_string_append_printf(chans, "%s,", rec->name);
	}

	if (chans->len > 0) {
		g_string_truncate(chans, chans->len-1);
		server->channels_join((SERVER_REC *)server, chans->str, TRUE);
	}

	g_string_free(chans, TRUE);
}

static void sig_recv_ping(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	json_t *message = json_object();
	json_object_set_new(message, "msg", json_string("pong"));
	g_queue_push_tail(server->message_queue, message);
	lws_callback_on_writable(server->wsi);
}

static void sig_recv_connected(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *credential;

	credential = json_object();
	json_object_set_new(credential, "resume", json_string(server->connrec->password));

	json_t *params = json_array();
	json_array_append_new(params, credential);

	callback = rocketchat_result_callback_new(result_cb_login, NULL);
	rocketchat_call(server, "login", params, callback);
}

static void sig_recv_result(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	const char *id;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	id = json_string_value(json_object_get(json, "id"));
	callback = g_hash_table_lookup(server->result_callbacks, id);
	if (callback && callback->func) {
		callback->func(server, json, callback->userdata);
	}

	g_hash_table_remove(server->result_callbacks, id);
}

static void sig_recv_added(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	const char *collection = json_string_value(json_object_get(json, "collection"));
	if (!strcmp(collection, "users")) {
		json_t *fields = json_object_get(json, "fields");
		g_free(server->nick);
		server->nick = g_strdup(json_string_value(json_object_get(fields, "username")));
	}
}

static void sig_recv_changed(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	const char *collection = json_string_value(json_object_get(json, "collection"));
	if (!strcmp(collection, "stream-room-messages")) {
		gboolean isNew = TRUE;
		gboolean isOwn;
		const char *message_id;

		json_t *fields = json_object_get(json, "fields");
		json_t *args = json_object_get(fields, "args");
		json_t *message = json_array_get(args, 0);

		if (NULL != json_object_get(message, "replies")) {
			isNew = FALSE;
		}

		if (NULL != json_object_get(message, "reactions")) {
			isNew = FALSE;
		}

		if (NULL != json_object_get(message, "editedAt")) {
			isNew = FALSE;
		}

		if (NULL != json_object_get(message, "t")) {
			isNew = FALSE;
		}

		json_t *urls = json_object_get(message, "urls");
		if (isNew && urls) {
			json_t *value;
			size_t index;
			json_array_foreach(urls, index, value) {
				json_t *parsedUrl = json_object_get(value, "parsedUrl");
				if (parsedUrl) {
					isNew = FALSE;
					break;
				}
			}
		}

		message_id = json_string_value(json_object_get(message, "_id"));
		isOwn = g_hash_table_remove(server->sent_messages, message_id);

		if (isNew && !isOwn) {
			const char *nick, *rid;

			nick = json_string_value(json_object_get(json_object_get(message, "u"), "username"));
			rid = json_string_value(json_object_get(message, "rid"));
			if (!channel_find((SERVER_REC *)server, rid)) {
				server->channels_join((SERVER_REC *)server, rid, TRUE);
			} else {
				signal_emit("message public", 5, server, json_string_value(json_object_get(message, "msg")), nick, server->connrec->address, rid);
			}
		}
	} else if (!strcmp(collection, "stream-notify-user")) {
		json_t *fields, *args, *subscription;
		const char *eventName, *roomId, *arg1;

		fields = json_object_get(json, "fields");
		eventName = json_string_value(json_object_get(fields, "eventName"));
		if (g_str_has_suffix(eventName, "/subscriptions-changed")) {
			args = json_object_get(fields, "args");
			arg1 = json_string_value(json_array_get(args, 0));
			if (!strcmp(arg1, "inserted")) {
				subscription = json_array_get(args, 1);
				roomId = json_string_value(json_object_get(subscription, "rid"));
				rocketchat_subscribe(server, "stream-room-messages", roomId);
				if (!channel_find((SERVER_REC *)server, roomId)) {
					server->channels_join((SERVER_REC *)server, roomId, TRUE);
				}
			} else if (!strcmp(arg1, "removed")) {
				subscription = json_array_get(args, 1);
				roomId = json_string_value(json_object_get(subscription, "rid"));
				rocketchat_unsubscribe(server, "stream-room-messages", roomId);
			}
		}
	}
}

static void result_cb_getUsersOfRoom(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *records, *user;
	const char *roomId;
	CHANNEL_REC *channel;
	NICK_REC *own_nick;
	size_t index;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	roomId = json_string_value(userdata);
	channel = channel_find((SERVER_REC *)server, roomId);
	if (channel == NULL) {
		return;
	}

	result = json_object_get(json, "result");
	records = json_object_get(result, "records");
	json_array_foreach(records, index, user) {
		const char *username = json_string_value(json_object_get(user, "username"));
		if (NULL == nicklist_find(channel, username)) {
			NICK_REC *nick = g_new0(NICK_REC, 1);
			nick->nick = g_strconcat(username, NULL);
			nicklist_insert(channel, nick);
		}
	}

	own_nick = nicklist_find(channel, server->nick);
	if (own_nick == NULL) {
		own_nick = g_new0(NICK_REC, 1);
		own_nick->nick = g_strconcat(server->nick, NULL);
		nicklist_insert(channel, own_nick);
	}
	nicklist_set_own(channel, own_nick);

	channel->joined = TRUE;
	channel->names_got = TRUE;
	signal_emit("channel joined", 1, channel);
}

static void result_cb_joinRoom(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *params;
	const char *roomId;
	CHANNEL_REC *channel;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	roomId = json_string_value(userdata);
	channel = channel_find((SERVER_REC *)server, roomId);
	if (channel == NULL) {
		return;
	}

	error = json_object_get(json, "error");
	if (error) {
		channel_destroy(channel);
	} else {
		params = json_array();
		json_array_append_new(params, json_string(roomId));
		json_array_append_new(params, json_true());

		callback = rocketchat_result_callback_new(result_cb_getUsersOfRoom, json_string(roomId));
		rocketchat_call(server, "getUsersOfRoom", params, callback);

		rocketchat_subscribe(server, "stream-room-messages", roomId);
	}
}

static void result_cb_canAccessRoom(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *result, *params;
	const char *roomId, *name, *fname, *t;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	CHANNEL_REC *channel;

	roomId = json_string_value(userdata);
	channel = channel_find((SERVER_REC *)server, roomId);
	if (channel == NULL) {
		return;
	}

	if (NULL != json_object_get(json, "error")) {
		channel_destroy(channel);
		return;
	}

	result = json_object_get(json, "result");
	roomId = json_string_value(json_object_get(result, "_id"));
	name = json_string_value(json_object_get(result, "name"));
	fname = json_string_value(json_object_get(result, "fname"));
	t = json_string_value(json_object_get(result, "t"));

	if (fname) {
		channel_change_visible_name(channel, fname);
	} else if (name) {
		channel_change_visible_name(channel, name);
	}

	if (*t == 'c' || *t == 'p') {
		// Public or private channel
		params = json_array();
		json_array_append_new(params, json_string(roomId));

		callback = rocketchat_result_callback_new(result_cb_joinRoom, json_string(roomId));
		rocketchat_call(server, "joinRoom", params, callback);
	} else if (*t == 'd') {
		size_t index;
		json_t *usernames, *value;

		usernames = json_object_get(result, "usernames");
		json_array_foreach(usernames, index, value) {
			const char *username = json_string_value(value);
			if (NULL == nicklist_find(channel, username)) {
				NICK_REC *nick = g_new0(NICK_REC, 1);
				nick->nick = g_strdup(username);
				nicklist_insert(channel, nick);
			}
		}

		params = json_array();
		json_array_append_new(params, json_string(roomId));
		json_array_append_new(params, json_true());

		callback = rocketchat_result_callback_new(result_cb_getUsersOfRoom, json_string(roomId));
		rocketchat_call(server, "getUsersOfRoom", params, callback);
		rocketchat_subscribe(server, "stream-room-messages", roomId);
	}
}

static CHATNET_REC *rocketchat_create_chatnet(void)
{
	return g_new0(CHATNET_REC, 1);
}

static SERVER_SETUP_REC *rocketchat_create_server_setup(void)
{
	return g_new0(SERVER_SETUP_REC, 1);
}

static SERVER_CONNECT_REC *rocketchat_create_server_connect(void)
{
	return g_new0(SERVER_CONNECT_REC, 1);
}

static CHANNEL_SETUP_REC *rocketchat_create_channel_setup(void)
{
	return g_new0(CHANNEL_SETUP_REC, 1);
}

static void rocketchat_destroy_server_connect(SERVER_CONNECT_REC *conn)
{
}

static void
rocketchat_channels_join(SERVER_REC *serverrec, const char *data, int automatic)
{
	ROCKETCHAT_SERVER_REC *server = (ROCKETCHAT_SERVER_REC *)serverrec;
	void *free_arg;
	char *channels;
	gchar **chanlist, **chan;
	CHANNEL_REC *channel;

	if (*data == '\0') {
		return;
	}

	if (!cmd_get_params(data, &free_arg, 1, &channels)) {
		return;
	}

	chanlist = g_strsplit(channels, ",", -1);
	for (chan = chanlist; *chan != NULL; chan++) {
		gchar *channel_name = g_strdup(*chan);
		channel = channel_find((SERVER_REC *)server, channel_name);
		if (channel == NULL) {
			json_t *params;
			ROCKETCHAT_RESULT_CALLBACK_REC *callback;

			channel = g_new0(CHANNEL_REC, 1);
			channel_init(channel, (SERVER_REC *)server, channel_name, NULL, automatic);

			params = json_array();
			json_array_append_new(params, json_string(channel_name));
			json_array_append_new(params, json_string(server->userId));

			callback = rocketchat_result_callback_new(result_cb_canAccessRoom, json_string(channel_name));
			rocketchat_call(server, "canAccessRoom", params, callback);

		}
		g_free(channel_name);
	}

	g_strfreev(chanlist);
	cmd_params_free(free_arg);
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
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *message, *params;

	message = json_object();
	json_object_set_new(message, "rid", json_string(target));
	json_object_set_new(message, "msg", json_string(msg));

	params = json_array();
	json_array_append_new(params, message);

	callback = rocketchat_result_callback_new(result_cb_sendMessage, NULL);
	rocketchat_call(server, "sendMessage", params, callback);
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

	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, "reason: %d", reason);

	//if (reason == LWS_CALLBACK_PROTOCOL_INIT || reason == LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED || reason == LWS_CALLBACK_WSI_CREATE) {
	//	return 0;
	//}

	server = lws_get_opaque_user_data(wsi);

	if (!server) {
		return 0;
	}
	if (server->disconnected) {
		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "disconnected %d", reason);
		return -1;
	}

	switch (reason) {
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			server->connection_lost = TRUE;
			server_connect_failed((SERVER_REC *)server, in);
			return -1;

		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "peer initiated close");
			server->connection_lost = TRUE;
			server_disconnect((SERVER_REC *)server);
			break;

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			lookup_servers = g_slist_remove(lookup_servers, server);
			server->connected = TRUE;
			server->wsi = wsi;
			server_connect_finished((SERVER_REC *)server);
			break;

		case LWS_CALLBACK_CLIENT_CLOSED:
			if (server) {
				server->connection_lost = TRUE;
				server_disconnect((SERVER_REC *)server);
			}
			break;

		case LWS_CALLBACK_CLIENT_WRITEABLE:
			json = g_queue_pop_head(server->message_queue);
			if (json) {
				size_t flags = JSON_COMPACT;
				size_t size = json_dumpb(json, NULL, 0, flags);
				if (size != 0) {
					char *buffer = g_malloc0(LWS_PRE + size + 1) + LWS_PRE;
					json_dumpb(json, buffer, size, flags);
					signal_emit("rocketchat json out", 2, server, buffer);
					lws_write(wsi, (unsigned char *)buffer, size, LWS_WRITE_TEXT);
					free(buffer - LWS_PRE);
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

				signal_emit("rocketchat json in", 2, server, server->buffer->str);
				json = json_loadb(server->buffer->str, server->buffer->len, 0, &json_error);
				g_string_truncate(server->buffer, 0);
				if (!json) {
					printtext(server, NULL, MSGLEVEL_CLIENTERROR, "json error: %s", json_error.text);
					break;
				}

				json_msg = json_object_get(json, "msg");
				if (json_msg) {
					const char *msg = json_string_value(json_msg);
					char *signal = g_strconcat("rocketchat recv ", msg, NULL);
					signal_emit(signal, 2, server, json);
					g_free(signal);
				}
				json_decref(json);
			} else {
				g_string_append_len(server->buffer, in, len);
			}
		}
		break;

		case LWS_CALLBACK_CLIENT_RECEIVE_PONG:
		case LWS_CALLBACK_CLIENT_FILTER_PRE_ESTABLISH:
		case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		case LWS_CALLBACK_WSI_CREATE:
		case LWS_CALLBACK_WSI_DESTROY:
			break;

		default:
			printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Unhandled lws callback reason: %d", reason);
	}

	return 0;
}

static const struct lws_protocols protocols[] = {
	{ "rocketchat", rocketchat_lws_callback, 0, 0, 0, NULL, 0 },
	{ NULL, NULL, 0, 0, 0, NULL, 0 }
};

static void rocketchat_lws_log_emit (int level, const char *line)
{
	printtext(NULL, NULL, MSGLEVEL_CLIENTCRAP, line);
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
	query_rec->server_tag = g_strdup(server_tag);
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

	signal_add("rocketchat recv ping", sig_recv_ping);
	signal_add("rocketchat recv connected", sig_recv_connected);
	signal_add("rocketchat recv result", sig_recv_result);
	signal_add("rocketchat recv added", sig_recv_added);
	signal_add("rocketchat recv changed", sig_recv_changed);

	rocketchat_servers_init();
	rocketchat_servers_reconnect_init();
	rocketchat_commands_init();

	module_register("rocketchat", "core");
}

void rocketchat_core_deinit(void)
{
	signal_remove("rocketchat recv ping", sig_recv_ping);
	signal_remove("rocketchat recv connected", sig_recv_connected);
	signal_remove("rocketchat recv result", sig_recv_result);
	signal_remove("rocketchat recv added", sig_recv_added);
	signal_remove("rocketchat recv changed", sig_recv_changed);

	rocketchat_servers_deinit();
	rocketchat_servers_reconnect_deinit();
	rocketchat_commands_deinit();

	signal_emit("chat protocol deinit", 1, chat_protocol_find(ROCKETCHAT_PROTOCOL_NAME));
	chat_protocol_unregister(ROCKETCHAT_PROTOCOL_NAME);
}

#ifdef IRSSI_ABI_VERSION
void rocketchat_core_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
