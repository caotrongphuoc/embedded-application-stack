#include "mongoose.h"
#include <signal.h>

static void ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_HTTP_MSG)
    {
        // struct mg_http_serve_opts opts = {.root_dir = "."};
        // mg_http_serve_dir(c, ev_data, &opts);
        // mg_http_reply(c, 200, "Content-Type: text/plain\r\n", "Hello, %s\n", "world");
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        if (mg_match(hm->uri, mg_str("/api/stats"), NULL))
        {
            printf("Received request for /api/stats\n");
        }
    }

    int main(int argc, char *argv[])
    {
        struct mg_mgr mgr;
        mg_mgr_init(&mgr);
        mg_http_listen(&mgr, "http://192.168.1.177:8000", ev_handler, NULL);
        for (;;)
        {
            mg_mgr_poll(&mgr, 1000);
        }
        mg_mgr_free(&mgr);
        return 0;
    }