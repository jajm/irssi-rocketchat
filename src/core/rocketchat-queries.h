#ifndef __ROCKETCHAT_QUERIES_H
#define __ROCKETCHAT_QUERIES_H

#include "queries.h"
#include "rocketchat-servers.h"

#define ROCKETCHAT_QUERY(query) \
	PROTO_CHECK_CAST(QUERY(query), ROCKETCHAT_QUERY_REC, chat_type, "rocketchat")

#define IS_ROCKETCHAT_QUERY(query) \
	(ROCKETCHAT_QUERY(query) ? TRUE : FALSE)

#define STRUCT_SERVER_REC ROCKETCHAT_SERVER_REC
struct _ROCKETCHAT_QUERY_REC {
#include "query-rec.h"
	char *rid;
	char *tmid;
};

#endif
