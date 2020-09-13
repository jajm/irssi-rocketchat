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
#include "signals.h"
#include "servers.h"

static void sig_server_connect_copy(SERVER_CONNECT_REC **dest, SERVER_CONNECT_REC *src)
{
	if (src->chat_type != ROCKETCHAT_PROTOCOL) {
		return;
	}

	*dest = g_new0(SERVER_CONNECT_REC, 1);
	(*dest)->chat_type = ROCKETCHAT_PROTOCOL;
}

void rocketchat_servers_reconnect_init(void)
{
	signal_add("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
}

void rocketchat_servers_reconnect_deinit(void)
{
	signal_remove("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
}
