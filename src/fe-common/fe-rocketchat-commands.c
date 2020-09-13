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
#include "rocketchat-result-callbacks.h"
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
}

void fe_rocketchat_commands_init(void)
{
	command_bind_rocketchat("rocketchat channels", NULL, (SIGNAL_FUNC)cmd_rocketchat_channels);
	command_bind_rocketchat("rocketchat users", NULL, (SIGNAL_FUNC)cmd_rocketchat_users);
}

void fe_rocketchat_commands_deinit(void)
{
	command_unbind("rocketchat channels", (SIGNAL_FUNC)cmd_rocketchat_channels);
	command_unbind("rocketchat users", (SIGNAL_FUNC)cmd_rocketchat_users);
}
