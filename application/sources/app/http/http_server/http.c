#include "mongoose.h"
#include <signal.h>

#define HTTP_LISTEN_URL "http://0.0.0.0:8000"

static volatile int s_quit = 0;
static void on_sigint(int sig)
{
	(void)sig;
	s_quit = 1;
}

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev == MG_EV_HTTP_MSG)
	{
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;

		if (mg_match(hm->uri, mg_str("/api/stats"), NULL))
		{
			MG_INFO(("GET %.*s", (int)hm->uri.len, hm->uri.buf));
			mg_http_reply(c, 200,
						  "Content-Type: application/json\r\n",
						  "{%m:%lu,%m:%m}\n",
						  MG_ESC("uptime_ms"), (unsigned long)mg_millis(),
						  MG_ESC("status"), MG_ESC("ok"));
		}
		else
		{
			mg_http_reply(c, 404,
						  "Content-Type: application/json\r\n",
						  "{%m:%m}\n",
						  MG_ESC("error"), MG_ESC("not found"));
		}
	}
}

int main(void)
{
	struct mg_mgr mgr;
	mg_log_set(MG_LL_INFO);
	mg_mgr_init(&mgr);

	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);

	if (mg_http_listen(&mgr, HTTP_LISTEN_URL, ev_handler, NULL) == NULL)
	{
		MG_ERROR(("Failed to listen on %s", HTTP_LISTEN_URL));
		mg_mgr_free(&mgr);
		return 1;
	}
	MG_INFO(("Listening on %s", HTTP_LISTEN_URL));

	while (!s_quit)
	{
		mg_mgr_poll(&mgr, 1000);
	}

	MG_INFO(("Shutting down"));
	mg_mgr_free(&mgr);
	return 0;
}
