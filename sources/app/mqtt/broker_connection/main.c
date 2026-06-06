#include "mongoose.h"
#include <signal.h>

static const char *s_url        = "mqtt://127.0.0.1:1883";
static const char *s_client_id  = "ctp";
static int         s_keepalive  = 60;
static volatile int s_signo     = 0;

static void on_signal(int signo) { s_signo = signo; }

static void fn(struct mg_connection *c, int ev, void *ev_data) {
	if (ev == MG_EV_OPEN) {
		MG_INFO(("[OPEN ] connection object created, id=%lu", c->id));
	}
	else if (ev == MG_EV_CONNECT) {
		MG_INFO(("[CONN ] TCP connected to broker"));
	}
	else if (ev == MG_EV_MQTT_OPEN) {
		/* ev_data tro toi int connack_status_code: 0 = success */
		int rc = *(int *) ev_data;
		MG_INFO(("[MQTT ] CONNACK received, rc=%d (%s)",
		         rc, rc == 0 ? "SUCCESS" : "FAILED"));
	}
	else if (ev == MG_EV_ERROR) {
		MG_ERROR(("[ERROR] %s", (char *) ev_data));
	}
	else if (ev == MG_EV_CLOSE) {
		MG_INFO(("[CLOSE] connection closed"));
	}
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_mqtt_opts opts;
	bool done = false;

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	mg_log_set(MG_LL_DEBUG);	/* MG_LL_ERROR / INFO / DEBUG / VERBOSE */
	mg_mgr_init(&mgr);

	memset(&opts, 0, sizeof(opts));
	opts.clean       = true;
	opts.qos         = 0;
	opts.version     = 4;	/* MQTT 3.1.1 */
	opts.keepalive   = s_keepalive;
	opts.client_id   = mg_str(s_client_id);

	MG_INFO(("Connecting to %s as %s ...", s_url, s_client_id));
	struct mg_connection *c = mg_mqtt_connect(&mgr, s_url, &opts, fn, &done);
	if (c == NULL) {
		MG_ERROR(("mg_mqtt_connect() failed"));
		mg_mgr_free(&mgr);
		return 1;
	}

	int ticks = 0;
	while (!done && s_signo == 0 && ticks < 50) {
		mg_mgr_poll(&mgr, 100);
		ticks++;
	}

	MG_INFO(("Done. Cleaning up..."));
	mg_mgr_free(&mgr);
	return 0;
}
