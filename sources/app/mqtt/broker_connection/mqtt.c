#include "mongoose.h"
#include <signal.h>

static volatile int s_quit = 0;
static void on_sigint(int sig) { (void) sig; s_quit = 1; }

static bool s_unsubbed = false;

static void fn(struct mg_connection *c, int ev, void *ev_data) {
	if (ev == MG_EV_MQTT_OPEN) {
		MG_INFO(("CONNACK rc=%d", *(int *) ev_data));

		struct mg_mqtt_opts opts;
		memset(&opts, 0, sizeof(opts));
		opts.topic = mg_str("demo/mqtt");
		opts.qos   = 1;
		// mg_mqtt_sub: gui goi SUBSCRIBE, dang ky nhan msg tu topic
		mg_mqtt_sub(c, &opts);
	}
	else if (ev == MG_EV_MQTT_CMD) {
		struct mg_mqtt_message *m = ev_data;
		MG_INFO(("CMD cmd=%u id=%u", m->cmd, m->id));

		if (m->cmd == MQTT_CMD_SUBACK) {
			struct mg_mqtt_opts opts;
			memset(&opts, 0, sizeof(opts));
			opts.topic   = mg_str("demo/mqtt");
			opts.message = mg_str("hello from mongoose");
			opts.qos     = 1;
			// mg_mqtt_pub: gui goi PUBLISH, tra ve packet ID
			mg_mqtt_pub(c, &opts);
		}
	}
	else if (ev == MG_EV_MQTT_MSG) {
		struct mg_mqtt_message *m = ev_data;
		MG_INFO(("RECV topic='%.*s' payload='%.*s'",
		         (int) m->topic.len, m->topic.buf,
		         (int) m->data.len, m->data.buf));

		if (!s_unsubbed) {
			s_unsubbed = true;
			struct mg_mqtt_opts opts;
			memset(&opts, 0, sizeof(opts));
			opts.topic = mg_str("demo/mqtt");
			// mg_mqtt_unsub: gui goi UNSUBSCRIBE, huy nhan msg tu topic
			mg_mqtt_unsub(c, &opts);
		}
	}
	else if (ev == MG_EV_CLOSE) {
		MG_INFO(("CLOSE"));
	}
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_mqtt_opts opts;

	mg_log_set(MG_LL_INFO);
	// mg_mgr_init: khoi tao event manager (epoll fd + ID counter)
	mg_mgr_init(&mgr);

	memset(&opts, 0, sizeof(opts));
	opts.client_id = mg_str("client");
	opts.clean     = true;
	opts.version   = 4;
	opts.keepalive = 60;
	opts.user      = mg_str("ctp");
	opts.pass      = mg_str("aloalo");
	opts.topic     = mg_str("demo/mqtt/will");
	opts.message   = mg_str("client's disconnected");
	opts.qos       = 1;

	signal(SIGINT,  on_sigint);
	signal(SIGTERM, on_sigint);

	// mg_mqtt_connect: mo TCP + gui goi CONNECT (kem Will + user/pass)
	struct mg_connection *c =
	    mg_mqtt_connect(&mgr, "mqtt://127.0.0.1:1883", &opts, fn, NULL);

	// mg_mgr_poll: 1 vong event (doi epoll, doc/ghi socket, goi callback)
	while (!s_quit) mg_mgr_poll(&mgr, 1000);

	struct mg_mqtt_opts disc_opts;
	memset(&disc_opts, 0, sizeof(disc_opts));
	// mg_mqtt_disconnect: gui goi DISCONNECT (2 byte) -> broker khong fire Will
	mg_mqtt_disconnect(c, &disc_opts);
	for (int i = 0; i < 5; i++) mg_mgr_poll(&mgr, 100);

	// mg_mgr_free: dong tat ca connection con lai + giai phong tai nguyen
	mg_mgr_free(&mgr);
	return 0;
}
