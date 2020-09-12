#include "module.h"
#include "commands.h"
#include "levels.h"
#include "printtext.h"
#include "rocketchat-servers.h"
#include "rocketchat-result-callbacks.h"
#include "rocketchat-protocol.h"
#include "libwebsockets.h"
#include "jansson.h"

#define command_bind_rocketchat(cmd, category, func) \
	command_bind_proto(cmd, ROCKETCHAT_PROTOCOL, category, func)

static void result_cb_spotlight(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	signal_emit("rocketchat recv result spotlight", 2, server, json);
}

static void result_cb_subscriptions(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	signal_emit("rocketchat recv result subscriptions", 2, server, json);
}

static void cmd_rocketchat(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	command_runsub("rocketchat", data, server, item);
}

static void cmd_rocketchat_spotlight(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	json_t *params;
	void *free_arg;
	char *text;
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	if (!cmd_get_params(data, &free_arg, 1, &text)) {
		return;
	}

	params = json_array();
	json_array_append_new(params, json_string(text));

	callback = rocketchat_result_callback_new(result_cb_spotlight, NULL);
	rocketchat_call(server, "spotlight", params, callback);

	cmd_params_free(free_arg);
}

static void cmd_rocketchat_subscriptions(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;

	callback = rocketchat_result_callback_new(result_cb_subscriptions, NULL);
	rocketchat_call(server, "subscriptions/get", json_array(), callback);
}

void rocketchat_commands_init(void)
{
	command_bind_rocketchat("rocketchat", NULL, (SIGNAL_FUNC)cmd_rocketchat);
	command_bind_rocketchat("rocketchat spotlight", NULL, (SIGNAL_FUNC)cmd_rocketchat_spotlight);
	command_bind_rocketchat("rocketchat subscriptions", NULL, (SIGNAL_FUNC)cmd_rocketchat_subscriptions);
}

void rocketchat_commands_deinit(void)
{
	command_unbind("rocketchat", (SIGNAL_FUNC)cmd_rocketchat);
	command_unbind("rocketchat spotlight", (SIGNAL_FUNC)cmd_rocketchat_spotlight);
	command_unbind("rocketchat subscriptions", (SIGNAL_FUNC)cmd_rocketchat_subscriptions);
}
