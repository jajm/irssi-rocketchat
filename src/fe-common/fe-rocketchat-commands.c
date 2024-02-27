/*
 *  Copyright (C) 2020  Julian Maurice
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "module.h"
#include "commands.h"
#include "printtext.h"
#include "levels.h"
#include "signals.h"
#include "rocketchat-protocol.h"
#include "rocketchat-queries.h"
#include "rocketchat-result-callbacks.h"
#include "rocketchat-message.h"
#include "rocketchat-room.h"
#include "rocketchat-channels.h"
#include "jansson.h"

#define command_bind_rocketchat(cmd, category, func) \
	command_bind_proto(cmd, ROCKETCHAT_PROTOCOL, category, func)

static void result_cb_browseChannels(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *channels, *channel;
	const char *id, *name, *fname;
	size_t index;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	channels = json_object_get(result, "results");
	json_array_foreach(channels, index, channel) {
		id = json_string_value(json_object_get(channel, "_id"));
		name = json_string_value(json_object_get(channel, "name"));
		fname = json_string_value(json_object_get(channel, "fname"));

		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "%s (ID: %s)", fname ? fname : name, id);
	}
}

static void result_cb_loadHistory(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *messages, *message;
	const char *target;
	size_t index;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	target = json_string_value(json_object_get(userdata, "target"));

	result = json_object_get(json, "result");
	messages = json_object_get(result, "messages");

	printtext(server, target, MSGLEVEL_CLIENTCRAP|MSGLEVEL_NEVER, "History:");
	for (index = json_array_size(messages); index > 0; index--) {
		const char *username;
		GDateTime *datetime;
		char *msg, *datetime_formatted;
		json_int_t ts;

		message = json_array_get(messages, index - 1);

		username = json_string_value(json_object_get(json_object_get(message, "u"), "username"));
		msg = rocketchat_format_message(server, message);

		ts = json_integer_value(json_object_get(json_object_get(message, "ts"), "$date"));
		datetime = g_date_time_new_from_unix_local(ts / 1000);
		datetime_formatted = g_date_time_format(datetime, "%F %T");

		printtext(server, target, MSGLEVEL_CLIENTCRAP|MSGLEVEL_NEVER, "%s <%s> %s", datetime_formatted, username, msg);

		g_free(msg);
		g_free(datetime_formatted);
		g_date_time_unref(datetime);
	}
	printtext(server, target, MSGLEVEL_CLIENTCRAP|MSGLEVEL_NEVER, "End of History");
}


static void cmd_rocketchat_channels(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *params, *param;

	param = json_object();
	json_object_set_new(param, "page", json_integer(0));
	json_object_set_new(param, "offset", json_integer(0));
	json_object_set_new(param, "limit", json_integer(100));

	params = json_array();
	json_array_append_new(params, param);

	callback = rocketchat_result_callback_new(result_cb_browseChannels, NULL);
	rocketchat_call(server, "browseChannels", params, callback);
}

static void cmd_rocketchat_users(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *params, *param;
	void *free_arg;
	char *text;

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_GETREST, &text)) {
		return;
	}

	param = json_object();
	json_object_set_new(param, "text", json_string(text));
	json_object_set_new(param, "workspace", json_string("all"));
	json_object_set_new(param, "type", json_string("users"));
	json_object_set_new(param, "page", json_integer(0));
	json_object_set_new(param, "offset", json_integer(0));
	json_object_set_new(param, "limit", json_integer(100));

	params = json_array();
	json_array_append_new(params, param);

	callback = rocketchat_result_callback_new(result_cb_browseChannels, NULL);
	rocketchat_call(server, "browseChannels", params, callback);

	cmd_params_free(free_arg);
}

static void cmd_rocketchat_history(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	json_t *params, *userdata;
	const char *target;
	const char *rid;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	void *free_me;
	char *countstr = NULL;
	int count = 10;

	if (!cmd_get_params(data, &free_me, 1, &countstr)) {
		return;
	}

	target = window_item_get_target(item);
	if (item->type == module_get_uniq_id_str("WINDOW ITEM TYPE", "QUERY")) {
		rid = ((ROCKETCHAT_QUERY_REC *)item)->rid;
	} else {
		rid = target;
	}

	if (countstr != NULL && *countstr != '\0') {
		count = atoi(countstr);
	}

	params = json_array();
	json_array_append(params, json_string(rid));
	json_array_append(params, json_null());
	json_array_append(params, json_integer(count));
	json_array_append(params, json_null());

	userdata = json_object();
	json_object_set_new(userdata, "target", json_string(target));

	callback = rocketchat_result_callback_new(result_cb_loadHistory, userdata);
	rocketchat_call(server, "loadHistory", params, callback);
}

static void cmd_rocketchat_subscribe(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	void *free_me = NULL;
	char *name = NULL;
	char *event = NULL;

	if (!cmd_get_params(data, &free_me, 2, &name, &event)) {
		return;
	}

	if (name && event) {
		rocketchat_subscribe(server, name, event);
	}
	cmd_params_free(free_me);
}

static void cmd_rocketchat_thread(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	void *free_me = NULL;
	char *tmid = NULL;
	char *text = NULL;
	char *msg;
	const char *rid;
	int is_channel = 0;
	ROCKETCHAT_ROOM_REC *room;

	if (!cmd_get_params(data, &free_me, 2 | PARAM_FLAG_GETREST, &tmid, &text)) {
		return;
	}

	is_channel = item->type == module_get_uniq_id_str("WINDOW ITEM TYPE", "CHANNEL");
	if (is_channel) {
		rid = item->name;
	} else {
		rid = ROCKETCHAT_QUERY(item)->rid;
	}

	room = g_hash_table_lookup(server->rooms, rid);
	g_return_if_fail(room != NULL);

	if (*tmid != '\0') {
		if (is_channel) {
			ROCKETCHAT_CHANNEL(item)->tmid = g_strdup(tmid);
		} else {
			ROCKETCHAT_QUERY(item)->tmid = g_strdup(tmid);
		}

		if (*text != '\0') {
			if (is_channel) {
				msg = g_strdup_printf("-channel %s %s", window_item_get_target(item), text);
			} else {
				msg = g_strdup_printf("-nick %s %s", window_item_get_target(item), text);
			}

			signal_emit("command msg", 3, msg, server, item);
			g_free(msg);

			if (is_channel) {
				g_free(ROCKETCHAT_CHANNEL(item)->tmid);
				ROCKETCHAT_CHANNEL(item)->tmid = NULL;
			} else {
				g_free(ROCKETCHAT_QUERY(item)->tmid);
				ROCKETCHAT_QUERY(item)->tmid = NULL;
			}
		} else {
			g_free(item->visible_name);
			item->visible_name = g_strjoin("/", room->fname ? room->fname : room->name, tmid, NULL);
			signal_emit("window item name changed", 1, item);
		}
	} else {
		if (is_channel) {
			g_free(ROCKETCHAT_CHANNEL(item)->tmid);
			ROCKETCHAT_CHANNEL(item)->tmid = NULL;
		} else {
			g_free(ROCKETCHAT_QUERY(item)->tmid);
			ROCKETCHAT_QUERY(item)->tmid = NULL;
		}

		g_free(item->visible_name);
		item->visible_name = g_strdup(room->fname ? room->fname : room->name);
		signal_emit("window item name changed", 1, item);
	}

	cmd_params_free(free_me);
}

void fe_rocketchat_commands_init(void)
{
	command_bind_rocketchat("rocketchat channels", NULL, (SIGNAL_FUNC)cmd_rocketchat_channels);
	command_bind_rocketchat("rocketchat users", NULL, (SIGNAL_FUNC)cmd_rocketchat_users);
	command_bind_rocketchat("rocketchat history", NULL, (SIGNAL_FUNC)cmd_rocketchat_history);
	command_bind_rocketchat("rocketchat subscribe", NULL, (SIGNAL_FUNC)cmd_rocketchat_subscribe);
	command_bind_rocketchat("rocketchat thread", NULL, (SIGNAL_FUNC)cmd_rocketchat_thread);
}

void fe_rocketchat_commands_deinit(void)
{
	command_unbind("rocketchat channels", (SIGNAL_FUNC)cmd_rocketchat_channels);
	command_unbind("rocketchat users", (SIGNAL_FUNC)cmd_rocketchat_users);
	command_unbind("rocketchat history", (SIGNAL_FUNC)cmd_rocketchat_history);
	command_unbind("rocketchat subscribe", (SIGNAL_FUNC)cmd_rocketchat_subscribe);
	command_unbind("rocketchat thread", (SIGNAL_FUNC)cmd_rocketchat_thread);
}
