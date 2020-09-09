#include "module.h"
#include "modules.h"
#include "signals.h"
#include "printtext.h"
#include "levels.h"

static void sig_json_out(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Tx: %s", json);
}

static void sig_json_in(ROCKETCHAT_SERVER_REC *server, const char *json)
{
	printtext(server, NULL, MSGLEVEL_CLIENTCRAP, "Rx: %s", json);
}

void fe_rocketchat_init(void)
{
	signal_add("rocketchat json out", sig_json_out);
	signal_add("rocketchat json in", sig_json_in);
	module_register("rocketchat", "fe");
}

void fe_rocketchat_deinit(void)
{
	signal_remove("rocketchat json out", sig_json_out);
	signal_remove("rocketchat json in", sig_json_in);
}

#ifdef IRSSI_ABI_VERSION
void fe_rocketchat_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
