#include "module.h"
#include "commands.h"
#include "printtext.h"
#include "levels.h"
#include "signals.h"
#include "rocketchat-protocol.h"
#include "rocketchat-result-callbacks.h"
#include "jansson.h"

#define command_bind_rocketchat(cmd, category, func) \
	command_bind_proto(cmd, ROCKETCHAT_PROTOCOL, category, func)

static void result_cb_channelsList(ROCKETCHAT_SERVER_REC *server, json_t *json, json_t *userdata)
{
	json_t *error, *result, *channels, *channel;
	const char *id, *name, *t;
	size_t index;

	error = json_object_get(json, "error");
	if (error) {
		return;
	}

	result = json_object_get(json, "result");
	channels = json_object_get(result, "channels");
	json_array_foreach(channels, index, channel) {
		id = json_string_value(json_object_get(channel, "_id"));
		name = json_string_value(json_object_get(channel, "name"));
		t = json_string_value(json_object_get(channel, "t"));

		printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "%s %s (%s)", id, name, *t == 'p' ? "private" : "public");
	}
}

static void cmd_rocketchat_channels(const char *data, ROCKETCHAT_SERVER_REC *server, WI_ITEM_REC *item)
{
	ROCKETCHAT_RESULT_CALLBACK_REC *callback;
	json_t *params;

	params = json_array();
	json_array_append(params, json_string(""));
	json_array_append(params, json_string(""));

	callback = rocketchat_result_callback_new(result_cb_channelsList, NULL);
	rocketchat_call(server, "channelsList", params, callback);
}

void fe_rocketchat_commands_init(void)
{
	command_bind_rocketchat("rocketchat channels", NULL, (SIGNAL_FUNC)cmd_rocketchat_channels);
}

void fe_rocketchat_commands_deinit(void)
{
	command_unbind("rocketchat channels", (SIGNAL_FUNC)cmd_rocketchat_channels);
}
