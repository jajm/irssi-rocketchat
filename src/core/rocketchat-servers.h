#ifndef __ROCKETCHAT_SERVERS_H
#define __ROCKETCHAT_SERVERS_H

#include "rocketchat.h"
#include "servers.h"
#include "chat-protocols.h"

#define ROCKETCHAT_SERVER(server) \
	PROTO_CHECK_CAST(SERVER(server), ROCKETCHAT_SERVER_REC, chat_type, "rocketchat")

#define IS_ROCKETCHAT_SERVER(server) \
	(ROCKETCHAT_SERVER(server) ? TRUE : FALSE)

#define STRUCT_SERVER_CONNECT_REC SERVER_CONNECT_REC
struct _ROCKETCHAT_SERVER_REC {
#include "server-rec.h"
	struct lws *wsi;
	GQueue *message_queue;
	GString *buffer;
	GHashTable *result_callbacks;
	GHashTable *rooms;
	char *userId;
};

void rocketchat_servers_init(void);
void rocketchat_servers_deinit(void);

#endif
