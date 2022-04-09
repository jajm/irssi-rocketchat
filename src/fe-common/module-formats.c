#include "module.h"
#include "formats.h"

FORMAT_REC fecommon_rocketchat_formats[] = {
	{ MODULE_NAME, "Rocket.Chat", 0, { 0 } },

	/* ---- */
	{ NULL, "Messages", 0, { 0 } },

	{ "own_msg", "{msgid $3}{ownmsgnick $2 {ownnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_thread", "{msgid $3}{tmid $4}{ownmsgnick $2 {ownnick $0}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "own_msg_channel", "{msgid $4}{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "own_msg_channel_thread", "{msgid $4}{tmid $5}{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "own_msg_private", "{msgid $2}{ownprivmsg msg $0}$1", 3, { 0, 0, 0 } },
	{ "own_msg_private_thread", "{msgid $2}{tmid $3}{ownprivmsg msg $0}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private_query", "{msgid $3}{ownprivmsgnick {ownprivnick $2}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private_query_thread", "{msgid $3}{tmid $4}{ownprivmsgnick {ownprivnick $2}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_me", "{msgid $3}{pubmsgmenick $2 {menick $0}}$1", 4, { 0, 0, 0, 0} },
	{ "pubmsg_me_thread", "{msgid $3}{tmid $4}{pubmsgmenick $2 {menick $0}}$1", 5, { 0, 0, 0, 0, 0} },
	{ "pubmsg_me_channel", "{msgid $4}{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_me_channel_thread", "{msgid $4}{tmid $5}{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight", "{msgid $4}{pubmsghinick $0 $3 $1}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_thread", "{msgid $4}{tmid $5}{pubmsghinick $0 $3 $1}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel", "{msgid $5}{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel_thread", "{msgid $5}{tmid $6}{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 7, { 0, 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg", "{msgid $3}{pubmsgnick $2 {pubnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_thread", "{msgid $3}{tmid $4}{pubmsgnick $2 {pubnick $0}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_channel", "{msgid $4}{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_channel_thread", "{msgid $4}{tmid $5}{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "msg_private", "{msgid $3}{privmsg $0 $1}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_thread", "{msgid $3}{tmid $4}{privmsg $0 $1}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "msg_private_query", "{msgid $3}{privmsgnick $0}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_query_thread", "{msgid $3}{tmid $4}{privmsgnick $0}$2", 5, { 0, 0, 0, 0, 0 } },

	{ NULL, NULL, 0, { 0 } }
};
