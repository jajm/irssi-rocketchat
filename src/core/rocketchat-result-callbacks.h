#ifndef __ROCKETCHAT_RESULT_CALLBACKS_H
#define __ROCKETCHAT_RESULT_CALLBACKS_H

#include "rocketchat.h"
#include "jansson.h"

typedef void (*ROCKETCHAT_RESULT_FUNC)(ROCKETCHAT_SERVER_REC *, json_t *, json_t *);

struct _ROCKETCHAT_RESULT_CALLBACK_REC {
	ROCKETCHAT_RESULT_FUNC func;
	json_t *userdata;
};

ROCKETCHAT_RESULT_CALLBACK_REC *rocketchat_result_callback_new(ROCKETCHAT_RESULT_FUNC func, json_t *userdata);
void rocketchat_result_callback_free(ROCKETCHAT_RESULT_CALLBACK_REC *result_callback);

#endif
