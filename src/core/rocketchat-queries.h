#ifndef __ROCKETCHAT_QUERIES_H
#define __ROCKETCHAT_QUERIES_H

#include "rocketchat-servers.h"

#define STRUCT_SERVER_REC ROCKETCHAT_SERVER_REC
typedef struct _ROCKETCHAT_QUERY_REC {
#include "query-rec.h"
} ROCKETCHAT_QUERY_REC;

#endif
