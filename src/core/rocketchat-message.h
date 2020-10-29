#ifndef __ROCKETCHAT_MESSAGE_H
#define __ROCKETCHAT_MESSAGE_H

#include "rocketchat.h"
#include "jansson.h"

char * rocketchat_format_message(ROCKETCHAT_SERVER_REC *server, json_t *message);

#endif
