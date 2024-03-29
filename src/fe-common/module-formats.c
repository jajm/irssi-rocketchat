#include "module.h"
#include "formats.h"

FORMAT_REC fecommon_rocketchat_formats[] = {
	{ MODULE_NAME, "Rocket.Chat", 0, { 0 } },

	/* ---- */
	{ NULL, "Messages", 0, { 0 } },

	{ "own_msg", "{ownmsgnick $2 {ownnick $0}}$1", 3, { 0, 0, 0 } },
	{ "own_msg_msgid", "{msgid $3}{ownmsgnick $2 {ownnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_thread", "{tmid $3}{ownmsgnick $2 {ownnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_thread_msgid", "{msgid $4}{tmid $3}{ownmsgnick $2 {ownnick $0}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "own_msg_channel", "{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 4, { 0, 0, 0, 0 } },
	{ "own_msg_channel_msgid", "{msgid $4}{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "own_msg_channel_thread", "{tmid $4}{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "own_msg_channel_thread_msgid", "{msgid $5}{tmid $4}{ownmsgnick $3 {ownnick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "own_msg_private", "{ownprivmsg msg $0}$1", 2, { 0, 0 } },
	{ "own_msg_private_msgid", "{msgid $2}{ownprivmsg msg $0}$1", 3, { 0, 0, 0 } },
	{ "own_msg_private_thread", "{tmid $2}{ownprivmsg msg $0}$1", 3, { 0, 0, 0 } },
	{ "own_msg_private_thread_msgid", "{msgid $3}{tmid $2}{ownprivmsg msg $0}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private_query", "{ownprivmsgnick {ownprivnick $2}}$1", 3, { 0, 0, 0 } },
	{ "own_msg_private_query_msgid", "{msgid $3}{ownprivmsgnick {ownprivnick $2}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private_query_thread", "{tmid $3}{ownprivmsgnick {ownprivnick $2}}$1", 4, { 0, 0, 0, 0 } },
	{ "own_msg_private_query_thread_msgid", "{msgid $4}{tmid $3}{ownprivmsgnick {ownprivnick $2}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_me", "{pubmsgmenick $2 {menick $0}}$1", 3, { 0, 0, 0} },
	{ "pubmsg_me_msgid", "{msgid $3}{pubmsgmenick $2 {menick $0}}$1", 4, { 0, 0, 0, 0} },
	{ "pubmsg_me_thread", "{tmid $3}{pubmsgmenick $2 {menick $0}}$1", 4, { 0, 0, 0, 0} },
	{ "pubmsg_me_thread_msgid", "{msgid $4}{tmid $3}{pubmsgmenick $2 {menick $0}}$1", 5, { 0, 0, 0, 0, 0} },
	{ "pubmsg_me_channel", "{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 4, {0, 0, 0, 0 } },
	{ "pubmsg_me_channel_msgid", "{msgid $4}{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_me_channel_thread", "{tmid $4}{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_me_channel_thread_msgid", "{msgid $5}{tmid $4}{pubmsgmenick $3 {menick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight", "{pubmsghinick $0 $3 $1}$2", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_hilight_msgid", "{msgid $4}{pubmsghinick $0 $3 $1}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_thread", "{tmid $4}{pubmsghinick $0 $3 $1}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_thread_msgid", "{msgid $5}{tmid $4}{pubmsghinick $0 $3 $1}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel", "{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel_msgid", "{msgid $5}{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel_thread", "{tmid $5}{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg_hilight_channel_thread_msgid", "{msgid $6}{tmid $5}{pubmsghinick $0 $4 $1{msgchannel $2}}$3", 7, { 0, 0, 0, 0, 0, 0, 0 } },
	{ "pubmsg", "{pubmsgnick $2 {pubnick $0}}$1", 3, { 0, 0, 0 } },
	{ "pubmsg_msgid", "{msgid $3}{pubmsgnick $2 {pubnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_thread", "{tmid $3}{pubmsgnick $2 {pubnick $0}}$1", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_thread_msgid", "{msgid $4}{tmid $3}{pubmsgnick $2 {pubnick $0}}$1", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_channel", "{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 4, { 0, 0, 0, 0 } },
	{ "pubmsg_channel_msgid", "{msgid $4}{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_channel_thread", "{tmid $4}{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "pubmsg_channel_thread_msgid", "{msgid $5}{tmid $4}{pubmsgnick $3 {pubnick $0}{msgchannel $1}}$2", 6, { 0, 0, 0, 0, 0, 0 } },
	{ "msg_private", "{privmsg $0 $1}$2", 3, { 0, 0, 0 } },
	{ "msg_private_msgid", "{msgid $3}{privmsg $0 $1}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_thread", "{tmid $3}{privmsg $0 $1}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_thread_msgid", "{msgid $4}{tmid $3}{privmsg $0 $1}$2", 5, { 0, 0, 0, 0, 0 } },
	{ "msg_private_query", "{privmsgnick $0}$2", 3, { 0, 0, 0 } },
	{ "msg_private_query_msgid", "{msgid $3}{privmsgnick $0}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_query_thread", "{tmid $3}{privmsgnick $0}$2", 4, { 0, 0, 0, 0 } },
	{ "msg_private_query_thread_msgid", "{msgid $4}{tmid $3}{privmsgnick $0}$2", 5, { 0, 0, 0, 0, 0 } },

	{ NULL, NULL, 0, { 0 } }
};
