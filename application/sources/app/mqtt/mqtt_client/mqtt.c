#include "mongoose.h"
#include <signal.h>

#define RECONNECT_MS 60000 // 1 min
#define MQTT_SERVER_URL "mqtts://broker.hivemq.com:8883"
#define CA_CERT_PATH "/etc/ssl/certs/ca-certificates.crt"
// #define CA_CERT_PATH "wrong-ca.pem"

static volatile int s_quit = 0;
static void on_sigint(int sig)
{
	(void)sig;
	s_quit = 1;
}

static struct mg_mgr s_mgr;
static struct mg_connection *s_conn = NULL;
static struct mg_mqtt_opts s_opts;
static uint64_t s_reconnect_at = 0;

static void fn(struct mg_connection *c, int ev, void *ev_data)
{
	if (ev == MG_EV_MQTT_OPEN)
	{
		MG_INFO(("CONNACK rc=%d", *(int *)ev_data));

		struct mg_mqtt_opts opts;
		memset(&opts, 0, sizeof(opts));

		opts.topic = mg_str("Request");
		opts.qos = 1;
		mg_mqtt_sub(c, &opts);

		opts.topic = mg_str("Signaling");
		opts.qos = 1;
		mg_mqtt_sub(c, &opts);

		opts.topic = mg_str("Status");
		opts.qos = 1;
		// mg_mqtt_sub: gui goi SUBSCRIBE, dang ky nhan msg tu topic
		mg_mqtt_sub(c, &opts);

		struct mg_mqtt_opts pub_opts = {
			.topic = mg_str("Status"),
			.message = mg_str("{\"status\":\"online\"}"),
			.qos = 1,
			.retain = true,
		};
		// mg_mqtt_pub: gui goi PUBLISH, tra ve packet ID
		mg_mqtt_pub(c, &pub_opts);
	}
	else if (ev == MG_EV_CONNECT)
	{
		if (c->is_tls)
		{
			struct mg_str ca = mg_file_read(&mg_fs_posix, CA_CERT_PATH);
			if (ca.len == 0)
			{
				MG_ERROR(("Failed to read CA cert from %s", CA_CERT_PATH));
				c->is_closing = 1;
				return;
			}
			struct mg_tls_opts opts = {
				.ca = ca,
				.name = mg_url_host(MQTT_SERVER_URL),
			};
			mg_tls_init(c, &opts);
			mg_free((void *)ca.buf);
		}
	}
	else if (ev == MG_EV_MQTT_CMD)
	{
		struct mg_mqtt_message *m = ev_data;
		MG_INFO(("CMD cmd=%u id=%u", m->cmd, m->id));
	}
	else if (ev == MG_EV_MQTT_MSG)
	{
		struct mg_mqtt_message *m = ev_data;
		MG_INFO(("RECV topic='%.*s' payload='%.*s'",
				 (int)m->topic.len, m->topic.buf,
				 (int)m->data.len, m->data.buf));

		if (mg_strcmp(m->topic, mg_str("Request")) == 0)
		{
			struct mg_mqtt_opts pub_opts;
			memset(&pub_opts, 0, sizeof(pub_opts));
			pub_opts.topic = mg_str("Response");
			pub_opts.message = mg_str("{\"status\":\"ok\"}");
			pub_opts.qos = 1;
			// mg_mqtt_pub: gui goi PUBLISH, tra ve packet ID
			mg_mqtt_pub(c, &pub_opts);
		}

		// Cloud gui lenh "STOP_SUB" -> huy dang ky topic
		if (mg_strcmp(m->data, mg_str("STOP_SUB")) == 0)
		{
			struct mg_mqtt_opts opts;
			memset(&opts, 0, sizeof(opts));
			opts.topic = m->topic;
			// mg_mqtt_unsub: gui goi UNSUBSCRIBE, huy nhan msg tu topic
			mg_mqtt_unsub(c, &opts);
		}
	}
	else if (ev == MG_EV_CLOSE)
	{
		MG_INFO(("CLOSE"));
		s_conn = NULL;
		if (!s_quit)
		{
			// mg_millis: lay thoi gian hien tai (ms tu Unix epoch)
			s_reconnect_at = mg_millis() + RECONNECT_MS;
			MG_INFO(("Will auto-reconnect in %d ms", RECONNECT_MS));
		}
	}
}

int main(void)
{
	mg_log_set(MG_LL_INFO);
	// mg_mgr_init: khoi tao event manager (epoll fd + ID counter)
	mg_mgr_init(&s_mgr);

	memset(&s_opts, 0, sizeof(s_opts));
	s_opts.client_id = mg_str("client");
	s_opts.clean = true;
	s_opts.version = 4;
	s_opts.keepalive = 60;
	s_opts.user = mg_str("ctp");
	s_opts.pass = mg_str("aloalo");
	s_opts.topic = mg_str("demo/mqtt/will");
	s_opts.message = mg_str("client's disconnected");
	s_opts.qos = 1;

	signal(SIGINT, on_sigint);
	signal(SIGTERM, on_sigint);

	// mg_mqtt_connect: mo TCP + gui goi CONNECT (kem Will + user/pass)
	if ((s_conn = mg_mqtt_connect(&s_mgr, MQTT_SERVER_URL, &s_opts, fn, NULL)) == NULL)
	{
		MG_ERROR(("Failed to connect to MQTT server"));
		s_reconnect_at = mg_millis() + RECONNECT_MS;
		MG_INFO(("Will auto-reconnect in %d ms", RECONNECT_MS));
	}

	// mg_mgr_poll: 1 vong event (doi epoll, doc/ghi socket, goi callback)
	while (!s_quit)
	{
		mg_mgr_poll(&s_mgr, 1000);

		// Reconnect check: connection da dong va da qua thoi diem retry
		if (s_conn == NULL && s_reconnect_at > 0 && mg_millis() >= s_reconnect_at)
		{
			s_reconnect_at = 0;
			s_conn = mg_mqtt_connect(&s_mgr, MQTT_SERVER_URL, &s_opts, fn, NULL);
		}
	}

	// Graceful disconnect
	if (s_conn != NULL)
	{
		struct mg_mqtt_opts disc_opts;
		memset(&disc_opts, 0, sizeof(disc_opts));
		// mg_mqtt_disconnect: gui goi DISCONNECT (2 byte) -> broker khong fire Will
		mg_mqtt_disconnect(s_conn, &disc_opts);
		for (int i = 0; i < 5; i++)
			mg_mgr_poll(&s_mgr, 100);
	}

	// mg_mgr_free: dong tat ca connection con lai + giai phong tai nguyen
	mg_mgr_free(&s_mgr);
	return 0;
}
