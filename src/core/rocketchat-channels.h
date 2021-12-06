#ifndef __ROCKETCHAT_CHANNELS_H
#define __ROCKETCHAT_CHANNELS_H

#include "channels.h"

#define ROCKETCHAT_CHANNEL(channel) \
	PROTO_CHECK_CAST(CHANNEL(channel), CHANNEL_REC, chat_type, "rocketchat")

#define IS_ROCKETCHAT_CHANNEL(channel) \
	(ROCKETCHAT_CHANNEL(channel) ? TRUE : FALSE)

#endif
