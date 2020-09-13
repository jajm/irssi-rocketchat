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
#include "rocketchat-result-callbacks.h"

ROCKETCHAT_RESULT_CALLBACK_REC *rocketchat_result_callback_new(ROCKETCHAT_RESULT_FUNC func, json_t *userdata)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *result_callback;

	result_callback = g_new0(ROCKETCHAT_RESULT_CALLBACK_REC, 1);
	result_callback->func = func;
	result_callback->userdata = userdata;

	return result_callback;
}

void rocketchat_result_callback_free(ROCKETCHAT_RESULT_CALLBACK_REC *result_callback)
{
	if (result_callback) {
		if (result_callback->userdata != NULL) {
			json_decref(result_callback->userdata);
		}
		g_free(result_callback);
	}
}
