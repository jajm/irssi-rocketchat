#ifndef __ROCKETCHAT_ROOM_H
#define __ROCKETCHAT_ROOM_H

#include "rocketchat.h"

struct _ROCKETCHAT_ROOM_REC {
	char *id;
	char type;
	char *name;
	char *fname;
};

ROCKETCHAT_ROOM_REC *rocketchat_room_new(const char *id, char type, const char *name, const char *fname);
void rocketchat_room_free(ROCKETCHAT_ROOM_REC *room);

#endif
