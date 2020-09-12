#ifndef __ROCKETCHAT_H
#define __ROCKETCHAT_H

#include "common.h"
#include "chat-protocols.h"

typedef struct _ROCKETCHAT_SERVER_REC ROCKETCHAT_SERVER_REC;
typedef struct _ROCKETCHAT_QUERY_REC ROCKETCHAT_QUERY_REC;
typedef struct _ROCKETCHAT_RESULT_CALLBACK_REC ROCKETCHAT_RESULT_CALLBACK_REC;

#define ROCKETCHAT_PROTOCOL_NAME "rocketchat"
#define ROCKETCHAT_PROTOCOL (chat_protocol_lookup(ROCKETCHAT_PROTOCOL_NAME))

#endif
