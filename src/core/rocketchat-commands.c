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
#include "levels.h"
#include "printtext.h"
#include "rocketchat-servers.h"
#include "rocketchat-result-callbacks.h"
#include "rocketchat-protocol.h"
#include "libwebsockets.h"
#include "jansson.h"

#define command_bind_rocketchat(cmd, category, func) \
	command_bind_proto(cmd, ROCKETCHAT_PROTOCOL, category, func)

static void result_cb_subscriptions(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	signal_emit("rocketchat recv result subscriptions", 2, server, json);
}

static void cmd_rocketchat(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	command_runsub("rocketchat", data, server, item);
}

static void cmd_rocketchat_subscriptions(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	callback = rocketchat_result_callback_new(result_cb_subscriptions, NULL);
	rocketchat_call(server, "subscriptions/get", json_array(), callback);
}

void rocketchat_commands_init(void)
{
	command_bind_rocketchat("rocketchat", NULL, (SIGNAL_FUNC)cmd_rocketchat);
	command_bind_rocketchat("rocketchat subscriptions", NULL, (SIGNAL_FUNC)cmd_rocketchat_subscriptions);
}

void rocketchat_commands_deinit(void)
{
	command_unbind("rocketchat", (SIGNAL_FUNC)cmd_rocketchat);
	command_unbind("rocketchat subscriptions", (SIGNAL_FUNC)cmd_rocketchat_subscriptions);
}
