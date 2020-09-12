#ifndef __ROCKETCHAT_PROTOCOL_H
#define __ROCKETCHAT_PROTOCOL_H

#include "rocketchat.h"
#include "jansson.h"

void rocketchat_call(ROCKETCHAT_SERVER_REC *server, const char *method, json_t *params, ROCKETCHAT_RESULT_CALLBACK_REC *callback);
void rocketchat_subscribe(ROCKETCHAT_SERVER_REC *server, const char *name, const char *event);
void rocketchat_unsubscribe(ROCKETCHAT_SERVER_REC *server, const char *name, const char *event);

#endif
