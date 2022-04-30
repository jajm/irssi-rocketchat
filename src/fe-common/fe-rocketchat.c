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
#include "printtext.h"
#include "themes.h"
#include "hilight-text.h"
#include "fe-messages.h"
#include "fe-queries.h"
#include "ignore.h"
#include "window-items.h"
#include "rocketchat-servers.h"
#include "rocketchat-channels.h"
#include "rocketchat-queries.h"
#include "rocketchat-protocol.h"
#include "rocketchat-message.h"
#include "rocketchat-result-callbacks.h"
#include "fe-rocketchat-commands.h"
#include "module-formats.h"
#include "jansson.h"

static void save_tmid(WI_ITEM_REC *item, const char *tmid)
{
	MODULE_WI_ITEM_REC *module_data;
	GQueue *last_thread_ids;
	GList *link;
	char *data;
	int max_tmids_saved;

	g_return_if_fail(item != NULL);
	g_return_if_fail(tmid != NULL);

	module_data = MODULE_DATA(item);
	last_thread_ids = module_data->last_thread_ids;
	link = g_queue_find_custom(last_thread_ids, tmid, (GCompareFunc) g_strcmp0);
	if (link) {
		g_queue_unlink(last_thread_ids, link);
		g_queue_push_head_link(last_thread_ids, link);
	} else {
		g_queue_push_head(last_thread_ids, g_strdup(tmid));
	}

	max_tmids_saved = settings_get_int("rocketchat_max_tmids_saved");
	while (g_queue_get_length(last_thread_ids) > max_tmids_saved) {
		data = g_queue_pop_tail(last_thread_ids);
		g_free(data);
	}
}

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

static void sig_rocketchat_message_public(SERVER_REC *server, const char *msg,
			       const char *nick, const char *msgid,
			       const char *target, const char *tmid)
{
	CHANNEL_REC *chanrec;
	int for_me, print_channel, print_msgid, level;
	char *color, *freemsg = NULL;
	char *nickmode = NULL;
	char *address = server->connrec->address;
	HILIGHT_REC *hilight;
	TEXT_DEST_REC dest;

	if (ignore_check(server, nick, address, target, msg, MSGLEVEL_PUBLIC)) {
		return;
	}

	int own = (!g_strcmp0(nick, server->nick));

	/* NOTE: this may return NULL if some channel is just closed with
	   /WINDOW CLOSE and server still sends the few last messages */
	chanrec = channel_find(server, target);

	for_me = !settings_get_bool("hilight_nick_matches") ? FALSE :
		!settings_get_bool("hilight_nick_matches_everywhere") ?
		nick_match_msg(chanrec, msg, server->nick) :
		nick_match_msg_everywhere(chanrec, msg, server->nick);
	hilight = (for_me || own) ? NULL :
		hilight_match_nick(server, target, nick, address, MSGLEVEL_PUBLIC, msg);
	color = (hilight == NULL) ? NULL : hilight_get_color(hilight);

	print_channel = chanrec == NULL ||
		!window_item_is_active((WI_ITEM_REC *) chanrec);
	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window_item_window((WI_ITEM_REC *) chanrec)->items->next != NULL)
		print_channel = TRUE;

	level = MSGLEVEL_PUBLIC;
	if (for_me) {
		level |= MSGLEVEL_HILIGHT;
	}
	if (own) {
		level |= MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT;
	}

	ignore_check_plus(server, nick, address, target, msg, &level, FALSE);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) chanrec, msg);

	print_msgid = settings_get_bool("rocketchat_print_msgid");

	format_create_dest(&dest, server, target, level, NULL);
	dest.address = address;
	dest.nick = nick;
	if (color != NULL) {
		/* highlighted nick */
		hilight_update_text_dest(&dest,hilight);
		if (!print_channel) {
			/* message to active channel in window */
			if (tmid) {
				if (print_msgid) {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_THREAD_MSGID, color, nick, msg, nickmode, tmid, msgid);
				} else {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_THREAD, color, nick, msg, nickmode, tmid);
				}
			} else {
				if (print_msgid) {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_MSGID, color, nick, msg, nickmode, msgid);
				} else {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT, color, nick, msg, nickmode);
				}
			}
		} else {
			/* message to not existing/active channel */
			if (tmid) {
				if (print_msgid) {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_CHANNEL_THREAD_MSGID, color, nick, target, msg, nickmode, tmid, msgid);
				} else {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_CHANNEL_THREAD, color, nick, target, msg, nickmode, tmid);
				}
			} else {
				if (print_msgid) {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_CHANNEL_MSGID, color, nick, target, msg, nickmode, msgid);
				} else {
					printformat_dest(&dest, ROCKETCHATTXT_PUBMSG_HILIGHT_CHANNEL, color, nick, target, msg, nickmode);
				}
			}
		}
	} else {
		if (!print_channel) {
			if (tmid) {
				if (print_msgid) {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_THREAD_MSGID :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_THREAD_MSGID : ROCKETCHATTXT_PUBMSG_THREAD_MSGID;
					printformat_dest(&dest, formatnum, nick, msg, nickmode, tmid, msgid);
				} else {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_THREAD :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_THREAD : ROCKETCHATTXT_PUBMSG_THREAD;
					printformat_dest(&dest, formatnum, nick, msg, nickmode, tmid);
				}
			} else {
				if (print_msgid) {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_MSGID :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_MSGID : ROCKETCHATTXT_PUBMSG_MSGID;
					printformat_dest(&dest, formatnum, nick, msg, nickmode, msgid);
				} else {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG :
							for_me ? ROCKETCHATTXT_PUBMSG_ME : ROCKETCHATTXT_PUBMSG;
					printformat_dest(&dest, formatnum, nick, msg, nickmode);
				}
			}
		} else {
			if (tmid) {
				if (print_msgid) {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_CHANNEL_THREAD_MSGID :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_CHANNEL_THREAD_MSGID : ROCKETCHATTXT_PUBMSG_CHANNEL_THREAD_MSGID;
					printformat_dest(&dest, formatnum, nick, target, msg, nickmode, tmid, msgid);
				} else {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_CHANNEL_THREAD :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_CHANNEL_THREAD : ROCKETCHATTXT_PUBMSG_CHANNEL_THREAD;
					printformat_dest(&dest, formatnum, nick, target, msg, nickmode, tmid);
				}
			} else {
				if (print_msgid) {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_CHANNEL_MSGID :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_CHANNEL_MSGID : ROCKETCHATTXT_PUBMSG_CHANNEL_MSGID;
					printformat_dest(&dest, formatnum, nick, target, msg, nickmode, msgid);
				} else {
					int formatnum = own ? ROCKETCHATTXT_OWN_MSG_CHANNEL :
							for_me ? ROCKETCHATTXT_PUBMSG_ME_CHANNEL : ROCKETCHATTXT_PUBMSG_CHANNEL;
					printformat_dest(&dest, formatnum, nick, target, msg, nickmode);
				}
			}
		}
	}

	if (chanrec && tmid) {
		save_tmid((WI_ITEM_REC *)chanrec, tmid);
	}

	g_free_not_null(nickmode);
	g_free_not_null(freemsg);
	g_free_not_null(color);
}

static void sig_rocketchat_message_private(SERVER_REC *server, const char *msg, const char *nick, const char *msgid, const char *target, const char *tmid)
{
	QUERY_REC *query;
	char *freemsg = NULL;
	int level = MSGLEVEL_MSGS;
	int print_msgid;
	const char *address = server->connrec->address;

	if (ignore_check(server, nick, address, NULL, msg, MSGLEVEL_MSGS)) {
		return;
	}

	int own = (!g_strcmp0(nick, server->nick));

	if (own) {
		level |= MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT;
	}

	query = privmsg_get_query(server, own ? target : nick, FALSE, MSGLEVEL_MSGS);

	if (query != NULL) {
		query->last_unread_msg = time(NULL);
	}

	if (settings_get_bool("emphasis")) {
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) query, msg);
	}

	ignore_check_plus(server, nick, address, NULL, msg, &level, FALSE);

	print_msgid = settings_get_bool("rocketchat_print_msgid");

	if (own) {
		if (query) {
			if (tmid) {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_QUERY_THREAD_MSGID, target, msg, server->nick, tmid, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_QUERY_THREAD, target, msg, server->nick, tmid);
				}
			} else {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_QUERY_MSGID, target, msg, server->nick, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_QUERY, target, msg, server->nick);
				}
			}
		} else {
			if (tmid) {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_THREAD_MSGID, target, msg, tmid, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_THREAD, target, msg, tmid);
				}
			} else {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE_MSGID, target, msg, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_OWN_MSG_PRIVATE, target, msg);
				}
			}
		}
	} else {
		if (query) {
			if (tmid) {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_QUERY_THREAD_MSGID, nick, address, msg, msgid, tmid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_QUERY_THREAD, nick, address, msg, tmid);
				}
			} else {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_QUERY_MSGID, nick, address, msg, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_QUERY, nick, address, msg);
				}
			}
		} else {
			if (tmid) {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_THREAD_MSGID, nick, address, msg, tmid, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_THREAD, nick, address, msg, tmid);
				}
			} else {
				if (print_msgid) {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE_MSGID, nick, address, msg, msgid);
				} else {
					printformat(server, target, level, ROCKETCHATTXT_MSG_PRIVATE, nick, address, msg);
				}
			}
		}
	}

	if (query && tmid) {
		save_tmid((WI_ITEM_REC *)query, tmid);
	}

	g_free_not_null(freemsg);
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

static void sig_message_own_public(SERVER_REC *server, const char *msg,
				   const char *target)
{
	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	/* Prevent sent message from being printed. It will be printed when
	 * sent back by the server */
	signal_stop();
}

static void sig_message_own_private(SERVER_REC *server, const char *msg,
				   const char *target)
{
	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	/* Prevent sent message from being printed. It will be printed when
	 * sent back by the server */
	signal_stop();
}

static void sig_query_created(QUERY_REC *query, int automatic)
{
	MODULE_WI_ITEM_REC *module_data;

	if (!IS_ROCKETCHAT_QUERY(query)) {
		return;
	}

	module_data = g_new0(MODULE_WI_ITEM_REC, 1);
	module_data->last_thread_ids = g_queue_new();
	MODULE_DATA_SET(query, module_data);
}

static void sig_query_destroyed(QUERY_REC *query)
{
	MODULE_WI_ITEM_REC *module_data;

	if (!IS_ROCKETCHAT_QUERY(query)) {
		return;
	}

	module_data = MODULE_DATA(query);
	if (module_data) {
		g_queue_free_full(module_data->last_thread_ids, g_free);
		g_free(module_data);
		MODULE_DATA_UNSET(query);
	}
}

static void sig_channel_created(CHANNEL_REC *channel, int automatic)
{
	MODULE_WI_ITEM_REC *module_data;

	if (!IS_ROCKETCHAT_CHANNEL(channel)) {
		return;
	}

	module_data = g_new0(MODULE_WI_ITEM_REC, 1);
	module_data->last_thread_ids = g_queue_new();
	MODULE_DATA_SET(channel, module_data);
}

static void sig_channel_destroyed(CHANNEL_REC *channel)
{
	MODULE_WI_ITEM_REC *module_data;

	if (!IS_ROCKETCHAT_CHANNEL(channel)) {
		return;
	}

	module_data = MODULE_DATA(channel);
	if (module_data) {
		g_queue_free_full(module_data->last_thread_ids, g_free);
		g_free(module_data);
		MODULE_DATA_UNSET(channel);
	}
}

static void sig_complete_command_rocketchat_thread(GList **list, WINDOW_REC *window, const char *word, const char *line, int *want_space)
{
	MODULE_WI_ITEM_REC *module_data;
	GList *tmp;

	if (!window->active) {
		return;
	}

	if (*line != '\0') {
		return;
	}

	module_data = MODULE_DATA(window->active);
	if (!module_data) {
		return;
	}

	tmp = module_data->last_thread_ids->head;
	while (tmp) {
		if (g_str_has_prefix(tmp->data, word)) {
			*list = g_list_append(*list, g_strdup(tmp->data));
		}
		tmp = tmp->next;
	}
	*want_space = TRUE;
}

void fe_rocketchat_init(void)
{
	theme_register(fecommon_rocketchat_formats);

	theme_set_default_abstract("msgid", "{comment $0}");
	theme_set_default_abstract("tmid", "{comment $0}");
	themes_reload();

	signal_add("rocketchat json out", sig_json_out);
	signal_add("rocketchat json in", sig_json_in);
	signal_add("rocketchat recv result subscriptions", sig_recv_result_subscriptions);
	signal_add("rocketchat error", sig_error);
	signal_add("rocketchat message public", sig_rocketchat_message_public);
	signal_add("rocketchat message private", sig_rocketchat_message_private);
	signal_add("complete word", sig_complete_word);
	signal_add("message own_public", sig_message_own_public);
	signal_add("message own_private", sig_message_own_private);
	signal_add("query created", sig_query_created);
	signal_add("query destroyed", sig_query_destroyed);
	signal_add("channel created", sig_channel_created);
	signal_add("channel destroyed", sig_channel_destroyed);
	signal_add("complete command rocketchat thread", sig_complete_command_rocketchat_thread);

	settings_add_bool("rocketchat", "rocketchat_debug", FALSE);
	settings_add_bool("rocketchat", "rocketchat_print_msgid", FALSE);
	settings_add_int("rocketchat", "rocketchat_max_tmids_saved", 10);

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
	signal_remove("rocketchat message public", sig_rocketchat_message_public);
	signal_remove("rocketchat message private", sig_rocketchat_message_private);
	signal_remove("complete word", sig_complete_word);
	signal_remove("message own_public", sig_message_own_public);
	signal_remove("message own_private", sig_message_own_private);
	signal_remove("query created", sig_query_created);
	signal_remove("query destroyed", sig_query_destroyed);
	signal_remove("channel created", sig_channel_created);
	signal_remove("channel destroyed", sig_channel_destroyed);
	signal_remove("complete command rocketchat thread", sig_complete_command_rocketchat_thread);

	theme_unregister();
}

#ifdef IRSSI_ABI_VERSION
void fe_rocketchat_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
