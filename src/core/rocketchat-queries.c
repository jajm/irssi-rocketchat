#include "module.h"
#include "modules.h"
#include "rocketchat-queries.h"

const char *
rocketchat_query_get_rid(ROCKETCHAT_QUERY_REC *query)
{
	MODULE_QUERY_REC * module_query;

	module_query = MODULE_DATA(query);

	return module_query->rid;
}
