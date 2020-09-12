#include "module.h"
#include "modules.h"
#include "signals.h"
#include "printtext.h"
#include "levels.h"
#include "channels.h"
#include "rocketchat-servers.h"
#include "rocketchat-protocol.h"
#include "rocketchat-result-callbacks.h"
#include "fe-rocketchat-commands.h"
#include "jansson.h"

static void sig_json_out(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Tx: %s", json);
}

static void sig_json_in(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Rx: %s", json);
}

static void sig_recv_result_spotlight(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	json_t *result, *users, *user, *rooms, *room;
	size_t index;
	GString *out;

	out = g_string_new(NULL);
	result = json_object_get(json, "result");

	users = json_object_get(result, "users");
	if (json_array_size(users) > 0) {
		g_string_append(out, "Users:\n");
		json_array_foreach(users, index, user) {
			const char *id = json_string_value(json_object_get(user, "_id"));
			const char *username = json_string_value(json_object_get(user, "username"));
			const char *name = json_string_value(json_object_get(user, "name"));

			g_string_append_printf(out, " %s %s (%s)\n", id, username, name);
		}
	}

	rooms = json_object_get(result, "rooms");
	if (json_array_size(rooms) > 0) {
		g_string_append(out, "Rooms:\n");
		json_array_foreach(rooms, index, room) {
			const char *id = json_string_value(json_object_get(room, "_id"));
			const char *name = json_string_value(json_object_get(room, "name"));

			g_string_append_printf(out, " %s %s\n", id, name);
		}
	}

	if (json_array_size(users) == 0 && json_array_size(rooms) == 0) {
		g_string_append(out, "No results");
	}

	printtext(server, NULL, MSGLEVEL_CRAP, out->str);
	g_string_free(out, TRUE);
}

static void sig_recv_result_subscriptions(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	json_t *result, *subscription;
	size_t index;
	GString *out;

	out = g_string_new(NULL);
	result = json_object_get(json, "result");
	json_array_foreach(result, index, subscription) {
		const char *t = json_string_value(json_object_get(subscription, "t"));
		const char *rid = json_string_value(json_object_get(subscription, "rid"));
		const char *name = json_string_value(json_object_get(subscription, "name"));
		const char *fname = json_string_value(json_object_get(subscription, "fname"));

		if (fname != NULL) {
			g_string_append_printf(out, "%s %s %s (%s)\n", t, rid, name, fname);
		} else {
			g_string_append_printf(out, "%s %s %s\n", t, rid, name);
		}
	}

	printtext(server, NULL, MSGLEVEL_CRAP, out->str);
	g_string_free(out, TRUE);
}

static void result_cb_loadHistory(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *messages, *message;
	size_t index;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	messages = json_object_get(result, "messages");

	for (index = json_array_size(messages); index > 0; index--) {
		const char *rid, *username, *msg;
		GDateTime *datetime;
		char *text, *datetime_formatted;
		json_int_t ts;

		message = json_array_get(messages, index - 1);

		rid = json_string_value(json_object_get(message, "rid"));
		username = json_string_value(json_object_get(json_object_get(message, "u"), "username"));
		msg = json_string_value(json_object_get(message, "msg"));

		ts = json_integer_value(json_object_get(json_object_get(message, "ts"), "$date"));
		datetime = g_date_time_new_from_unix_local(ts / 1000);
		datetime_formatted = g_date_time_format(datetime, "%F %T");

		text = g_strdup_printf("[%s] <%s> %s", datetime_formatted, username, msg);
		printtext(server, rid, MSGLEVEL_CLIENTCRAP, text);

		g_free(text);
		g_free(datetime_formatted);
		g_date_time_unref(datetime);
	}
}

static void sig_channel_joined(CHANNEL_REC *channel)
{
	json_t *params;
	ROCKETCHAT_SERVER_REC *server;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	if (!IS_ROCKETCHAT_SERVER(channel->server)) {
		return;
	}

	server = (ROCKETCHAT_SERVER_REC *)channel->server;

	params = json_array();
	json_array_append(params, json_string(channel->name));
	json_array_append(params, json_null());
	json_array_append(params, json_integer(10));
	json_array_append(params, json_null());

	callback = rocketchat_result_callback_new(result_cb_loadHistory, NULL);
	rocketchat_call(server, "loadHistory", params, callback);
}

void fe_rocketchat_init(void)
{
	signal_add("rocketchat json out", sig_json_out);
	signal_add("rocketchat json in", sig_json_in);
	signal_add("rocketchat recv result spotlight", sig_recv_result_spotlight);
	signal_add("rocketchat recv result subscriptions", sig_recv_result_subscriptions);
	signal_add("channel joined", sig_channel_joined);

	fe_rocketchat_commands_init();

	module_register("rocketchat", "fe");
}

void fe_rocketchat_deinit(void)
{
	fe_rocketchat_commands_deinit();

	signal_remove("rocketchat json out", sig_json_out);
	signal_remove("rocketchat json in", sig_json_in);
	signal_remove("rocketchat recv result spotlight", sig_recv_result_spotlight);
	signal_remove("rocketchat recv result subscriptions", sig_recv_result_subscriptions);
	signal_remove("channel joined", sig_channel_joined);
}

#ifdef IRSSI_ABI_VERSION
void fe_rocketchat_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
