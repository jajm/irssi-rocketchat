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
#include "rocketchat.h"
#include "rocketchat-room.h"

ROCKETCHAT_ROOM_REC *rocketchat_room_new(const char *id, char type, const char *name, const char *fname)
{
	ROCKETCHAT_ROOM_REC *room;

	room = g_new0(ROCKETCHAT_ROOM_REC, 1);
	room->id = g_strdup(id);
	room->type = type;
	if (name) {
		room->name = g_strdup(name);
	}
	if (fname) {
		room->fname = g_strdup(fname);
	}

	return room;
}

void rocketchat_room_free(ROCKETCHAT_ROOM_REC *room)
{
	if (room) {
		g_free(room->id);
		g_free(room->name);
		g_free(room->fname);
		g_free(room);
	}
}
