#include "rocketchat.h"
#include "rocketchat-servers.h"
#include "jansson.h"

char * rocketchat_format_message(ROCKETCHAT_SERVER_REC *server, json_t *message)
{
	json_t *msg;
	char *formatted_message = NULL;

	msg = json_object_get(message, "msg");
	if (0 == json_string_length(msg)) {
		json_t *attachments;

		attachments = json_object_get(message, "attachments");
		if (json_is_array(attachments) && json_array_size(attachments) > 0) {
			json_t *attachment;
			const char *title_link;
			gchar *port, *url;

			attachment = json_array_get(attachments, 0);
			title_link = json_string_value(json_object_get(attachment, "title_link"));

			if ( (server->connrec->use_tls && server->connrec->port != 443) || (!server->connrec->use_tls && server->connrec->port != 80) ) {
				port = g_strdup_printf(":%d", server->connrec->port);
			} else {
				port = g_strdup("");
			}

			formatted_message = g_strdup_printf("%s://%s%s%s",
				server->connrec->use_tls ? "https" : "http",
				server->connrec->address,
				port,
				title_link);

			g_free(port);
		}
	} else {
		formatted_message = g_strdup(json_string_value(msg));
	}

	return formatted_message;
}
