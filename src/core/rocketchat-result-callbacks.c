#include "module.h"
#include "rocketchat.h"
#include "rocketchat-result-callbacks.h"

ROCKETCHAT_RESULT_CALLBACK_REC *rocketchat_result_callback_new(ROCKETCHAT_RESULT_FUNC func, json_t *userdata)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *result_callback;

	result_callback = g_new0(ROCKETCHAT_RESULT_CALLBACK_REC, 1);
	result_callback->func = func;
	result_callback->userdata = userdata;

	return result_callback;
}

void rocketchat_result_callback_free(ROCKETCHAT_RESULT_CALLBACK_REC *result_callback)
{
	if (result_callback) {
		if (result_callback->userdata != NULL) {
			json_decref(result_callback->userdata);
		}
		g_free(result_callback);
	}
}
