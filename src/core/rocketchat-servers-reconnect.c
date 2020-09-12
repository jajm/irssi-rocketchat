#include "module.h"
#include "signals.h"
#include "servers.h"

static void sig_server_connect_copy(SERVER_CONNECT_REC **dest, SERVER_CONNECT_REC *src)
{
	if (src->chat_type != ROCKETCHAT_PROTOCOL) {
		return;
	}

	*dest = g_new0(SERVER_CONNECT_REC, 1);
	(*dest)->chat_type = ROCKETCHAT_PROTOCOL;
}

void rocketchat_servers_reconnect_init(void)
{
	signal_add("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
}

void rocketchat_servers_reconnect_deinit(void)
{
	signal_remove("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
}
