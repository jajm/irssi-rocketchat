#include "module.h"
#include "signals.h"
#include "printtext.h"
#include "levels.h"
#include "rocketchat-servers.h"
#include "jansson.h"
#include "libwebsockets.h"

static void sig_server_disconnected(ROCKETCHAT_SERVER_REC *server)
{
	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	// Force connection close
	lws_callback_on_writable(server->wsi);
}

static void sig_server_destroyed(ROCKETCHAT_SERVER_REC *server)
{
	g_queue_free_full(server->message_queue, (GDestroyNotify)json_decref);
	server->message_queue = NULL;

	g_string_free(server->buffer, TRUE);
	server->buffer = NULL;

	lws_set_opaque_user_data(server->wsi, NULL);
}

void rocketchat_servers_init(void)
{
	signal_add_last("server disconnected", sig_server_disconnected);
	signal_add_last("server destroyed", sig_server_destroyed);
}

void rocketchat_servers_deinit(void)
{
	signal_remove("server disconnected", sig_server_disconnected);
	signal_remove("server destroyed", sig_server_destroyed);
}
