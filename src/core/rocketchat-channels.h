#ifndef __ROCKETCHAT_CHANNELS_H
#define __ROCKETCHAT_CHANNELS_H

#include "channels.h"
#include "rocketchat-servers.h"

#define ROCKETCHAT_CHANNEL(channel) \
	PROTO_CHECK_CAST(CHANNEL(channel), ROCKETCHAT_CHANNEL_REC, chat_type, "rocketchat")

#define IS_ROCKETCHAT_CHANNEL(channel) \
	(ROCKETCHAT_CHANNEL(channel) ? TRUE : FALSE)

#define STRUCT_SERVER_REC ROCKETCHAT_SERVER_REC
struct _ROCKETCHAT_CHANNEL_REC {
#include "channel-rec.h"
	char *tmid;
};

#endif
