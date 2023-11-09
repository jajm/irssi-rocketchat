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
#include "rocketchat-channels.h"
#include "rocketchat-commands.h"
#include "rocketchat-result-callbacks.h"
#include "rocketchat-room.h"
#include "rocketchat-protocol.h"
#include "rocketchat-message.h"
#include "libwebsockets.h"
#include "jansson.h"

static void result_cb_sendMessage(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	if (!result) {
		signal_emit("rocketchat error", 3, server, NULL, "Failed to send message");
	}
}

static void result_cb_createDirectMessage(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result;
	const char *username;
	QUERY_REC *query;

	username = json_string_value(json_object_get(userdata, "username"));
	query = query_find((SERVER_REC *)server, username);

	error = json_object_get(json, "error");
	if (error) {
		query_destroy(query);
		json_t *message = json_object_get(error, "message");
		signal_emit("rocketchat error", 3, server, NULL, json_string_value(message));
		return;
	}

	result = json_object_get(json, "result");
	signal_emit("rocketchat direct message created", 3, server, username, result);
}


static void result_cb_get_subscriptions_after_login(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *result, *subscription;
	size_t index;
	CHANNEL_REC *channel;

	result = json_object_get(json, "result");
	json_array_foreach(result, index, subscription) {
		const char *rid = json_string_value(json_object_get(subscription, "rid"));
		const char *type = json_string_value(json_object_get(subscription, "t"));
		const char *name = json_string_value(json_object_get(subscription, "name"));
		const char *fname = json_string_value(json_object_get(subscription, "fname"));
		ROCKETCHAT_ROOM_REC *room = rocketchat_room_new(rid, *type, name, fname);
		g_hash_table_insert(server->rooms, g_strdup(rid), room);

		channel = channel_find((SERVER_REC *)server, rid);
		if (channel && (fname || name)) {
			channel_change_visible_name(channel, fname ? fname : name);
		}

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

static void
result_cb_getRoomIdByNameOrId(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result;
	const char *rid, *channel_name;
	CHANNEL_REC *channel;

	channel_name = json_string_value(json_object_get(userdata, "channel_name"));
	channel = channel_find((SERVER_REC *)server, channel_name);

	error = json_object_get(json, "error");
	if (error) {
		channel_destroy(channel);
		return;
	}

	result = json_object_get(json, "result");
	if (result) {
		rid = json_string_value(result);
		if (channel) {
			channel_change_name(channel, rid);
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
		signal_emit("rocketchat error", 3, server, NULL, json_string_value(json_object_get(error, "message")));
		return;
	}

	params = json_array();
	json_array_append_new(params, json_string(roomId));
	json_array_append_new(params, json_true());

	callback = rocketchat_result_callback_new(result_cb_getUsersOfRoom, json_string(roomId));
	rocketchat_call(server, "getUsersOfRoom", params, callback);

	rocketchat_subscribe(server, "stream-room-messages", roomId);
}

static void
result_cb_getRoomById(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *params;
	json_int_t usersCount;
	const char *rid, *name, *fname, *type;
	ROCKETCHAT_ROOM_REC *room;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	CHANNEL_REC *channel;
	int ischannel;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	if (result) {
		rid = json_string_value(json_object_get(result, "_id"));
		name = json_string_value(json_object_get(result, "name"));
		fname = json_string_value(json_object_get(result, "fname"));
		type = json_string_value(json_object_get(result, "t"));
		usersCount = json_integer_value(json_object_get(result, "usersCount"));

		ischannel = *type != 'd' || usersCount > 2 || (name && strchr(name, ',') != NULL);
		room = g_hash_table_lookup(server->rooms, rid);
		if (room) {
			if (name) {
				g_free(room->name);
				room->name = g_strdup(name);
			}
			if (fname) {
				g_free(room->fname);
				room->fname = g_strdup(fname);
			}
			room->type = *type;
		} else {
			room = rocketchat_room_new(rid, *type, name, fname);
			g_hash_table_insert(server->rooms, g_strdup(rid), room);
		}

		channel = channel_find((SERVER_REC *)server, room->id);

		if (ischannel && (name || fname)) {
			channel_change_visible_name(channel, fname ? fname : name);
		}

		if (room->type != 'd') {
			params = json_array();
			json_array_append_new(params, json_string(room->id));

			callback = rocketchat_result_callback_new(result_cb_joinRoom, json_string(room->id));
			rocketchat_call(server, "joinRoom", params, callback);
		} else if (ischannel) {
			json_t *usernames, *value;
			size_t index;

			usernames = json_object_get(result, "usernames");
			if (json_is_array(usernames)) {
				NICK_REC *own_nick;

				json_array_foreach(usernames, index, value) {
					const char *username = json_string_value(value);
					if (NULL == nicklist_find(channel, username)) {
						NICK_REC *nick = g_new0(NICK_REC, 1);
						nick->nick = g_strdup(username);
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
		}
	}
}

static void sig_query_created(ROCKETCHAT_QUERY_REC *query, void *automatic_p)
{
	if (!IS_ROCKETCHAT_QUERY(query)) {
		return;
	}
}

static void sig_query_destroyed(ROCKETCHAT_QUERY_REC *query)
{
	if (!IS_ROCKETCHAT_QUERY(query)) {
		return;
	}

	g_free(query->rid);
}

static void sig_channel_created(CHANNEL_REC *channel, void *automatic_p)
{
	json_t *params, *userdata;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	ROCKETCHAT_SERVER_REC *server;

	if (!IS_ROCKETCHAT_CHANNEL(channel)) {
		return;
	}

	server = ROCKETCHAT_SERVER(channel->server);

	params = json_array();
	json_array_append_new(params, json_string(channel->name));

	userdata = json_object();
	json_object_set_new(userdata, "channel_name", json_string(channel->name));

	callback = rocketchat_result_callback_new(result_cb_getRoomIdByNameOrId, userdata);
	rocketchat_call(server, "getRoomIdByNameOrId", params, callback);
}

static void
sig_channel_name_changed(CHANNEL_REC *channel)
{
	json_t *params;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	if (!IS_ROCKETCHAT_CHANNEL(channel)) {
		return;
	}

	params = json_array();
	json_array_append_new(params, json_string(channel->name));

	callback = rocketchat_result_callback_new(result_cb_getRoomById, NULL);
	rocketchat_call((ROCKETCHAT_SERVER_REC *)channel->server, "getRoomById", params, callback);
}

static void sig_direct_message_created(ROCKETCHAT_SERVER_REC *server, const char *username, json_t *result)
{
	const char *rid, *name, *fname;
	char type;
	ROCKETCHAT_QUERY_REC *query;
	ROCKETCHAT_ROOM_REC *room;

	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	rid = json_string_value(json_object_get(result, "rid"));
	name = json_string_value(json_object_get(result, "name"));
	fname = json_string_value(json_object_get(result, "fname"));
	type = 'd';

	room = g_hash_table_lookup(server->rooms, rid);
	if (!room) {
		room = rocketchat_room_new(rid, type, name, fname);
		g_hash_table_insert(server->rooms, g_strdup(rid), room);
	}

	query = (ROCKETCHAT_QUERY_REC *)query_find((SERVER_REC *)server, username);
	if (query) {
		g_free(query->rid);
		query->rid = g_strdup(rid);
	}

	rocketchat_subscribe(server, "stream-room-messages", rid);
}

static void sig_recv_ping(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	json_t *message = json_object();
	json_object_set_new(message, "msg", json_string("pong"));
	g_queue_push_tail(server->message_queue, message);
	lws_callback_on_writable(server->wsi);
}

static void sig_recv_connected(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *credential;

	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

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

	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	id = json_string_value(json_object_get(json, "id"));
	callback = g_hash_table_lookup(server->result_callbacks, id);
	if (callback && callback->func) {
		callback->func(server, json, callback->userdata);
	}

	g_hash_table_remove(server->result_callbacks, id);
}

static void sig_recv_added(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	const char *collection = json_string_value(json_object_get(json, "collection"));
	if (!strcmp(collection, "users")) {
		json_t *fields = json_object_get(json, "fields");
		g_free(server->nick);
		server->nick = g_strdup(json_string_value(json_object_get(fields, "username")));
	}
}

static void sig_recv_changed(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	const char *collection = json_string_value(json_object_get(json, "collection"));
	if (!strcmp(collection, "stream-room-messages")) {
		gboolean isNew = TRUE;
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

		json_t *t = json_object_get(message, "t");
		if (t != NULL) {
			const char *t_str = json_string_value(t);

			if (!g_strcmp0(t_str, "uj")) {
				const char *rid = json_string_value(json_object_get(message, "rid"));
				const char *username = json_string_value(json_object_get(message, "msg"));
				CHANNEL_REC *channel = channel_find((SERVER_REC *)server, rid);
				if (channel && NULL == nicklist_find(channel, username)) {
					NICK_REC *nick = g_new0(NICK_REC, 1);
					nick->nick = g_strdup(username);
					nicklist_insert(channel, nick);
				}
			} else if (!g_strcmp0(t_str, "ul")) {
				const char *rid = json_string_value(json_object_get(message, "rid"));
				const char *username = json_string_value(json_object_get(message, "msg"));
				CHANNEL_REC *channel = channel_find((SERVER_REC *)server, rid);
				if (channel) {
					NICK_REC *nick = nicklist_find(channel, username);
					if (nick) {
						nicklist_remove(channel, nick);
					}
				}
			} else if (!g_strcmp0(t_str, "r")) {
				const char *rid = json_string_value(json_object_get(message, "rid"));
				const char *name = json_string_value(json_object_get(message, "msg"));
				CHANNEL_REC *channel = channel_find((SERVER_REC *)server, rid);
				if (channel) {
					channel_change_visible_name(channel, name);
				}
			}
			return;
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

		if (isNew) {
			const char *nick, *rid;

			nick = json_string_value(json_object_get(json_object_get(message, "u"), "username"));
			rid = json_string_value(json_object_get(message, "rid"));
			ROCKETCHAT_ROOM_REC *room = g_hash_table_lookup(server->rooms, rid);
			if (room) {
				const char *tmid = json_string_value(json_object_get(message, "tmid"));

				if (room->type == 'd' && strchr(room->name, ',') == NULL) {
					char *msg = rocketchat_format_message(server, message);
					if (msg != NULL) {
						signal_emit("rocketchat message private", 6, server, msg, nick, message_id, room->name, tmid);
						g_free(msg);
					}
				} else {
					char *msg;

					if (!channel_find((SERVER_REC *)server, rid)) {
						const char *visible_name = room->fname ? room->fname : room->name;
						CHAT_PROTOCOL(server)->channel_create(SERVER(server), rid, visible_name, TRUE);
					}

					msg = rocketchat_format_message(server, message);
					if (msg != NULL) {
						signal_emit("rocketchat message public", 6, server, msg, nick, message_id, rid, tmid);
						g_free(msg);
					}
				}
			}
		}
	} else if (!strcmp(collection, "stream-notify-user")) {
		json_t *fields, *args, *subscription;
		const char *eventName, *roomId, *arg1, *type, *name, *fname;
		ROCKETCHAT_ROOM_REC *room;

		fields = json_object_get(json, "fields");
		eventName = json_string_value(json_object_get(fields, "eventName"));
		if (g_str_has_suffix(eventName, "/subscriptions-changed")) {
			args = json_object_get(fields, "args");
			arg1 = json_string_value(json_array_get(args, 0));
			if (!strcmp(arg1, "inserted")) {
				subscription = json_array_get(args, 1);
				roomId = json_string_value(json_object_get(subscription, "rid"));
				type = json_string_value(json_object_get(subscription, "t"));
				name = json_string_value(json_object_get(subscription, "name"));
				fname = json_string_value(json_object_get(subscription, "fname"));
				room = g_hash_table_lookup(server->rooms, roomId);
				if (room) {
					if (name) {
						g_free(room->name);
						room->name = g_strdup(name);
					}
					if (fname) {
						g_free(room->fname);
						room->fname = g_strdup(fname);
					}
				} else {
					room = rocketchat_room_new(roomId, *type, name, fname);
					g_hash_table_insert(server->rooms, g_strdup(roomId), room);
				}

				rocketchat_subscribe(server, "stream-room-messages", roomId);
			} else if (!strcmp(arg1, "removed")) {
				subscription = json_array_get(args, 1);
				roomId = json_string_value(json_object_get(subscription, "rid"));
				rocketchat_unsubscribe(server, "stream-room-messages", roomId);
			}
		}
	}
}

static int resub(void *data)
{
	ROCKETCHAT_SERVER_REC *server;
	gchar *name, *event;
	GPtrArray *args = data;

	server = g_ptr_array_index(args, 0);
	g_return_val_if_fail(IS_ROCKETCHAT_SERVER(server), G_SOURCE_REMOVE);

	name = g_ptr_array_index(args, 1);
	event = g_ptr_array_index(args, 2);

	if (name && event) {
		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Resubscribing to %s %s", name, event);
		rocketchat_subscribe(server, name, event);
	}

	g_free(name);
	g_free(event);
	g_ptr_array_free(args, TRUE);

	return G_SOURCE_REMOVE;
}

static void sig_recv_nosub(ROCKETCHAT_SERVER_REC *server, json_t *json)
{
	const char *id, *message, *error_str;
	json_t *error, *details;
	json_int_t timeToReset;
	gchar **tokens;
	gint interval;

	g_return_if_fail(IS_ROCKETCHAT_SERVER(server));

	error = json_object_get(json, "error");
	if (error) {
		id = json_string_value(json_object_get(json, "id"));
		message = json_string_value(json_object_get(error, "message"));

		error_str = json_string_value(json_object_get(error, "error"));
		if (error_str && !strcmp(error_str, "too-many-requests")) {
			tokens = g_strsplit(id, ":", 3);
			if (tokens[1] && tokens[2]) {
				GPtrArray *args = g_ptr_array_new();
				g_ptr_array_add(args, server);
				g_ptr_array_add(args, g_strdup(tokens[1]));
				g_ptr_array_add(args, g_strdup(tokens[2]));

				details = json_object_get(error, "details");
				timeToReset = json_integer_value(json_object_get(details, "timeToReset"));
				interval = (timeToReset / 1000) + 1;
				printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Subscription failed because of rate limit. Retrying in %d seconds [%s]", interval, id);

				g_timeout_add_seconds(interval, resub, args);
			}
			g_strfreev(tokens);
		} else {
			printtext(server, NULL, MSGLEVEL_CLIENTERROR, "nosub: %s [%s]", message, id);
		}
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
		channel = channel_find((SERVER_REC *)server, *chan);
		if (channel == NULL) {
			CHAT_PROTOCOL(server)->channel_create(SERVER(server), *chan, NULL, automatic);
		}
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
rocketchat_create_direct_message(ROCKETCHAT_SERVER_REC *server, const char *username)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *params, *userdata;

	params = json_array();
	json_array_append_new(params, json_string(username));

	userdata = json_object();
	json_object_set_new(userdata, "username", json_string(username));

	callback = rocketchat_result_callback_new(result_cb_createDirectMessage, userdata);
	rocketchat_call(server, "createDirectMessage", params, callback);
}

static void
rocketchat_send_message(SERVER_REC *server_rec, const char *target, const char *msg, int target_type)
{
	ROCKETCHAT_SERVER_REC *server = (ROCKETCHAT_SERVER_REC *)server_rec;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	ROCKETCHAT_QUERY_REC *query;
	ROCKETCHAT_CHANNEL_REC *channel;
	json_t *message, *params;
	const char *rid;
	const char *tmid = NULL;

	if (target_type == SEND_TARGET_NICK) {
		query = (ROCKETCHAT_QUERY_REC *)query_find(server_rec, target);
		rid = query->rid;
		tmid = query->tmid;
	} else {
		channel = (ROCKETCHAT_CHANNEL_REC *)channel_find(server_rec, target);
		rid = target;
		tmid = channel->tmid;
	}

	message = json_object();
	json_object_set_new(message, "rid", json_string(rid));
	json_object_set_new(message, "msg", json_string(msg));
	if (tmid) {
		json_object_set_new(message, "tmid", json_string(tmid));
	}

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
				server_meta_clear_all(SERVER(server));
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
	ROCKETCHAT_CHANNEL_REC *channel;

	channel = g_new0(ROCKETCHAT_CHANNEL_REC, 1);
	channel_init((CHANNEL_REC *)channel, server, name, visible_name, automatic);

	return (CHANNEL_REC *)channel;
}

static QUERY_REC *
rocketchat_query_create(const char *server_tag, const char *data, int automatic)
{
	ROCKETCHAT_QUERY_REC *query;

	query = g_new0(ROCKETCHAT_QUERY_REC, 1);
	query->chat_type = ROCKETCHAT_PROTOCOL;
	query->name = g_strdup(data);
	query->server_tag = g_strdup(server_tag);
	query_init((QUERY_REC*)query, automatic);

	rocketchat_create_direct_message(query->server, data);

	return (QUERY_REC*)query;
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

	signal_add("query created", sig_query_created);
	signal_add("query destroyed", sig_query_destroyed);
	signal_add("channel created", sig_channel_created);
	signal_add("channel name changed", sig_channel_name_changed);

	signal_add_first("rocketchat direct message created", sig_direct_message_created);

	signal_add("rocketchat recv ping", sig_recv_ping);
	signal_add("rocketchat recv connected", sig_recv_connected);
	signal_add("rocketchat recv result", sig_recv_result);
	signal_add("rocketchat recv added", sig_recv_added);
	signal_add("rocketchat recv changed", sig_recv_changed);
	signal_add("rocketchat recv nosub", sig_recv_nosub);

	rocketchat_servers_init();
	rocketchat_servers_reconnect_init();
	rocketchat_commands_init();

	module_register("rocketchat", "core");
}

void rocketchat_core_deinit(void)
{
	signal_remove("query created", sig_query_created);
	signal_remove("query destroyed", sig_query_destroyed);
	signal_remove("channel created", sig_channel_created);
	signal_remove("channel name changed", sig_channel_name_changed);

	signal_remove("rocketchat direct message created", sig_direct_message_created);

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
