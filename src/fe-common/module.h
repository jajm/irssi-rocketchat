#define MODULE_NAME "fe-common/rocketchat"

#include "common.h"
#include "rocketchat.h"

typedef struct {
	GQueue *last_thread_ids;
} MODULE_WI_ITEM_REC;
