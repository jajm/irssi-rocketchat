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
#include "rocketchat-servers.h"
#include "libwebsockets.h"
#include "jansson.h"

void rocketchat_call(ROCKETCHAT_SERVER_REC *server, const char *method, json_t *params, ROCKETCHAT_RESULT_CALLBACK_REC *callback)
{
	json_t *message;
	char *id;

	id = g_uuid_string_random();

	message = json_object();
	json_object_set_new(message, "msg", json_string("method"));
	json_object_set_new(message, "method", json_string(method));
	json_object_set_new(message, "id", json_string(id));
	json_object_set_new(message, "params", params);

	if (callback) {
		g_hash_table_insert(server->result_callbacks, id, callback);
	} else {
		g_free(id);
	}

	g_queue_push_tail(server->message_queue, message);
	lws_callback_on_writable(server->wsi);
}

static char * rocketchat_subscription_id(const char *name, const char *event)
{
	return g_strconcat("sub:", name, ":", event, NULL);
}

void rocketchat_subscribe(ROCKETCHAT_SERVER_REC *server, const char *name, const char *event)
{
	char *id;
	json_t *params, *message;

	id = rocketchat_subscription_id(name, event);

	params = json_array();
	json_array_append_new(params, json_string(event));
	json_array_append_new(params, json_false());

	message = json_object();
	json_object_set_new(message, "msg", json_string("sub"));
	json_object_set_new(message, "name", json_string(name));
	json_object_set_new(message, "id", json_string(id));
	json_object_set_new(message, "params", params);

	g_free(id);
	g_queue_push_tail(server->message_queue, message);
	lws_callback_on_writable(server->wsi);
}

void rocketchat_unsubscribe(ROCKETCHAT_SERVER_REC *server, const char *name, const char *event)
{
	json_t *message;
	char *id;

	id = rocketchat_subscription_id(name, event);

	message = json_object();
	json_object_set_new(message, "msg", json_string("unsub"));
	json_object_set_new(message, "id", json_string(id));

	g_free(id);
	g_queue_push_tail(server->message_queue, message);
	lws_callback_on_writable(server->wsi);
}
