#include "module.h"
#include "signals.h"
#include "printtext.h"
#include "levels.h"
#include "rocketchat-servers.h"
#include "jansson.h"
#include "libwebsockets.h"

static void sig_server_connected(ROCKETCHAT_SERVER_REC *server)
{
	json_t *json, *support;

	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	server->message_queue = g_queue_new();
	server->buffer = g_string_new(NULL);
	server->result_callbacks = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	support = json_array();
	json_array_append_new(support, json_string("1"));

	json = json_object();
	json_object_set_new(json, "msg", json_string("connect"));
	json_object_set_new(json, "version", json_string("1"));
	json_object_set_new(json, "support", support);

	g_queue_push_tail(server->message_queue, json);
	lws_callback_on_writable(server->wsi);
}

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
	if (!IS_ROCKETCHAT_SERVER(server)) {
		return;
	}

	g_queue_free_full(server->message_queue, (GDestroyNotify)json_decref);
	server->message_queue = NULL;

	g_string_free(server->buffer, TRUE);
	server->buffer = NULL;

	g_hash_table_destroy(server->result_callbacks);
	server->result_callbacks = NULL;

	lws_set_opaque_user_data(server->wsi, NULL);
}

void rocketchat_servers_init(void)
{
	signal_add_first("server connected", sig_server_connected);
	signal_add_last("server disconnected", sig_server_disconnected);
	signal_add_last("server destroyed", sig_server_destroyed);
}

void rocketchat_servers_deinit(void)
{
	signal_remove("server connected", sig_server_connected);
	signal_remove("server disconnected", sig_server_disconnected);
	signal_remove("server destroyed", sig_server_destroyed);
}
