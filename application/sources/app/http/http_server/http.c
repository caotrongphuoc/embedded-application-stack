#include "mongoose.h"
#include <signal.h>

#define HTTP_LISTEN_URL "http://0.0.0.0:8000"

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev == MG_EV_HTTP_MSG)
	{
		struct mg_http_message *hm = (struct mg_http_message *)ev_data;
		if (mg_match(hm->uri, mg_str("/api/stats"), NULL))
		{
			MG_INFO(("Received request for /api/stats"));
		}
	}
}

int main(void)
{
	struct mg_mgr mgr;
	mg_log_set(MG_LL_INFO);
	mg_mgr_init(&mgr);
	mg_http_listen(&mgr, HTTP_LISTEN_URL, ev_handler, NULL);
	for (;;)
	{
		mg_mgr_poll(&mgr, 1000);
	}
	mg_mgr_free(&mgr);
	return 0;
}
