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
#include "modules.h"
#include "signals.h"
#include "printtext.h"
#include "levels.h"
#include "channels.h"
#include "nicklist.h"
#include "settings.h"
#include "rocketchat-servers.h"
#include "rocketchat-protocol.h"
#include "rocketchat-message.h"
#include "rocketchat-result-callbacks.h"
#include "fe-rocketchat-commands.h"
#include "jansson.h"

static void sig_json_out(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	if (settings_get_bool("rocketchat_debug")) {
		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Tx: %s", json);
	}
}

static void sig_json_in(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	if (settings_get_bool("rocketchat_debug")) {
		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Rx: %s", json);
	}
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

static void sig_error(ROCKETCHAT_SERVER_REC *server, const char *target, const char *msg)
{
	printtext(server, target, MSGLEVEL_CRAP, msg);
}

static void sig_complete_word(GList **complist, WINDOW_REC *window, char *word, char *linestart, int *want_space)
{
	ROCKETCHAT_SERVER_REC *server;
	CHANNEL_REC *channel;
	NICK_REC *nick;
	GSList *nicks, *tmp;

	server = (ROCKETCHAT_SERVER_REC *)window->active_server;
	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	channel = CHANNEL(window->active);
	if (!channel) {
		return;
	}

	nicks = nicklist_getnicks(channel);
	for (tmp = nicks; tmp != NULL; tmp = tmp->next) {
		nick = tmp->data;
		if (0 == g_ascii_strncasecmp(nick->nick, word, strlen(word))) {
			*complist = g_list_append(*complist, g_strdup(nick->nick));
			if (*linestart != '/') {
				*complist = g_list_append(*complist, g_strconcat("@", nick->nick, NULL));
			}
		}
	}

	if (g_str_has_prefix("here", word)) {
		*complist = g_list_append(*complist, g_strdup("@here"));
	}
	if (g_str_has_prefix("all", word)) {
		*complist = g_list_append(*complist, g_strdup("@all"));
	}

	if (*complist != NULL) {
		signal_stop();
	}
}

void fe_rocketchat_init(void)
{
	signal_add("rocketchat json out", sig_json_out);
	signal_add("rocketchat json in", sig_json_in);
	signal_add("rocketchat recv result subscriptions", sig_recv_result_subscriptions);
	signal_add("rocketchat error", sig_error);
	signal_add("complete word", sig_complete_word);

	settings_add_bool("rocketchat", "rocketchat_debug", FALSE);

	fe_rocketchat_commands_init();

	module_register("rocketchat", "fe");
}

void fe_rocketchat_deinit(void)
{
	fe_rocketchat_commands_deinit();

	signal_remove("rocketchat json out", sig_json_out);
	signal_remove("rocketchat json in", sig_json_in);
	signal_remove("rocketchat recv result subscriptions", sig_recv_result_subscriptions);
	signal_remove("rocketchat error", sig_error);
	signal_remove("complete word", sig_complete_word);
}

#ifdef IRSSI_ABI_VERSION
void fe_rocketchat_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
