<h1 align="center">MQTT Study Report with the Mongoose Library</h1>

Welcome to the MQTT study report of this repository. The goal is to work through the Mongoose MQTT API on a Linux host, rewrite it into a single self-contained C file, connect it to a local Mosquitto broker, and cover every core MQTT feature end-to-end before porting to a real board.

---

## Table of Contents

- [I. Purpose](#i-purpose)
- [II. Folder Structure](#ii-folder-structure)
- [III. Mongoose API Reference](#iii-mongoose-api-reference)
- [IV. Callback Events](#iv-callback-events)
- [V. MQTT Command Codes in Logs](#v-mqtt-command-codes-in-logs)
- [VI. In-Code Configuration](#vi-in-code-configuration)
- [VII. Program Flow](#vii-program-flow)
- [VIII. Build and Run](#viii-build-and-run)
- [IX. Test Cases](#ix-test-cases)
  - [Test 1 - Subscribe, Receive, Unsubscribe on Command](#test-1---subscribe-receive-unsubscribe-on-command)
  - [Test 2 - Last Will Fires on Abrupt Disconnect](#test-2---last-will-fires-on-abrupt-disconnect)
  - [Test 3 - Graceful Disconnect: Will Does Not Fire](#test-3---graceful-disconnect-will-does-not-fire)
  - [Test 4 - Auto-Reconnect After Broker Restart](#test-4---auto-reconnect-after-broker-restart)
  - [Test 5 - Username / Password Authentication](#test-5---username--password-authentication)
  - [Test 6 - Publish: Client Sends Messages to the Broker](#test-6---publish-client-sends-messages-to-the-broker)
  - [Test 7 - TLS: Connecting via `mqtts://` with a Self-Signed CA](#test-7---tls-connecting-via-mqtts-with-a-self-signed-ca)
- [X. Conclusion](#x-conclusion)
- [XI. References](#xi-references)

---

## I. Purpose

Learn how to use the MQTT API of the Mongoose library, rewrite it into a single C file, connect to a broker (currently a local Mosquitto broker, since the code has not been ported to a board yet), and exercise the essential features: `sub`, `pub`, `unsub`, graceful `disconnect`, Last Will (abrupt disconnect / crash), auto-reconnect, and username/password authentication.

All source code lives in `mqtt.c`. The Mongoose library sits in the `lib` folder.

---

## II. Folder Structure

Relative to the repository root (`embedded-application-stack/`), the MQTT module lives at:

```
application/sources/app/mqtt/mqtt_client/
├── lib/
│   ├── mongoose.c          # upstream library
│   └── mongoose.h          # upstream library
├── certs/                  # self-signed CA + server cert (used in Test 7)
├── mqtt.c                  # main source
├── Makefile                # gcc build, links mongoose.c and mbedTLS
├── mosq_auth.conf          # Mosquitto config with auth enabled (used in Test 5)
└── mosq_tls.conf           # Mosquitto config with TLS enabled (used in Test 7)
```

This report (`mqtt-reporting.md`) lives at `docs/` under the repository root.

---

## III. Mongoose API Reference

The "Source Reference" column links directly to the declaration in `mongoose.h` and the implementation in `mongoose.c` for quick lookup:

| API | Role | Source Reference |
| --- | --- | --- |
| `mg_mgr_init(mgr)` | Initialize the event manager (epoll fd, ID counter) | [h:1805](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1805) - [c:7262](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L7262) |
| `mg_mgr_poll(mgr, ms)` | Run one event-loop iteration, waiting up to `ms` milliseconds | [h:1804](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1804) - [c:13777](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L13777) |
| `mg_mgr_free(mgr)` | Close every remaining connection and free resources | [h:1806](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1806) - [c:7242](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L7242) |
| `mg_mqtt_connect(mgr, url, opts, fn, fn_data)` | Open TCP and send the CONNECT packet | [h:3088](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3088) - [c:6961](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6961) |
| `mg_mqtt_pub(c, opts)` | Send a PUBLISH packet; returns the `uint16_t` packet ID | [h:3094](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3094) - [c:6719](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6719) |
| `mg_mqtt_sub(c, opts)` | Send a SUBSCRIBE packet (one topic per call) | [h:3095](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3095) - [c:6779](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6779) |
| `mg_mqtt_unsub(c, opts)` | Send an UNSUBSCRIBE packet | [h:3096](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3096) - [c:6783](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6783) |
| `mg_mqtt_disconnect(c, opts)` | Send a DISCONNECT packet (2 bytes: `0xE0 0x00`) | [h:3102](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3102) - [c:6944](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6944) |
| `mg_str("...")` | Build a `struct mg_str` with `*buf` (data pointer) and `len` (size). Macro that expands to `mg_str_s()` | [h:1200](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1200) - [c:13938](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L13938) |
| `mg_strcmp(s1, s2)` | Compare two `mg_str` values | [h:1205](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1205) - [c:13977](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L13977) |
| `mg_millis()` | Current time in milliseconds since Unix epoch | [h:2943](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L2943) - [c:24783](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L24783) |
| `mg_log_set(level)` | Set log level (`MG_LL_ERROR/INFO/DEBUG/VERBOSE`). Macro that assigns `mg_log_level` | [h:1287](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1287) |
| `MG_INFO((...))` | Macro for INFO-level logging | [h:1333](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1333) |

---

## IV. Callback Events

```c
static void fn(struct mg_connection *c, int ev, void *ev_data);
```

| Event | When It Fires | `ev_data` | Reference |
| --- | --- | --- | --- |
| `MG_EV_MQTT_OPEN` | CONNACK received | `int *` = return code (0 = OK) | [h:1709](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1709) |
| `MG_EV_MQTT_CMD` | Any MQTT packet received | `struct mg_mqtt_message *` | [h:1707](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1707) |
| `MG_EV_MQTT_MSG` | PUBLISH received (message to a subscribed topic) | `struct mg_mqtt_message *` | [h:1708](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1708) |
| `MG_EV_CLOSE` | Connection closed | `NULL` | [h:1701](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L1701) |

---

## V. MQTT Command Codes in Logs

When `MG_EV_MQTT_CMD` fires, the `m->cmd` field identifies the packet type:

| `cmd` | MQTT Packet Name | Meaning | Reference |
| --- | --- | --- | --- |
| 2 | CONNACK | Broker acknowledges the connect | [h:2996](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L2996) |
| 3 | PUBLISH | Incoming message on a subscribed topic | [h:2997](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L2997) |
| 4 | PUBACK | Broker acknowledges a QoS >= 1 message we just published | [h:2998](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L2998) |
| 9 | SUBACK | Broker acknowledges a SUBSCRIBE packet | [h:3003](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3003) |
| 11 | UNSUBACK | Broker acknowledges an UNSUBSCRIBE packet | [h:3005](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3005) |
| 13 | PINGRESP | Broker replies to a keep-alive PING | [h:3007](../application/sources/app/mqtt/mqtt_client/lib/mongoose.h#L3007) |

---

## VI. In-Code Configuration

These parameters are hard-coded directly in the source for convenience during the study phase:

| Parameter | Value | Location |
| --- | --- | --- |
| Broker URL | `mqtt://127.0.0.1:1883` | 2nd argument of `mg_mqtt_connect` |
| Client ID | `client` | `opts.client_id` |
| Username | `ctp` | `opts.user` |
| Password | `aloalo` | `opts.pass` |
| Keep-alive | 60 seconds | `opts.keepalive` |
| MQTT version | 4 (MQTT 3.1.1) | `opts.version` |
| Clean session | `true` | `opts.clean` |
| Will topic | `demo/mqtt/will` | `opts.topic` |
| Will message | `client's disconnected` | `opts.message` |
| Will QoS | 1 | `opts.qos` |
| Subscribed topics | `Request`, `Signaling`, `Status` | Inside the `MG_EV_MQTT_OPEN` handler |
| Reconnect delay | 60000 ms (1 minute) | Macro `RECONNECT_MS` |

---

## VII. Program Flow

```
START
  |
  |-> mg_mgr_init                   create epoll fd
  |
  |-> signal(SIGINT/SIGTERM, ...)   register handler for Ctrl+C (used to test graceful disconnect)
  |
  |-> mg_mqtt_connect               open TCP and send CONNECT (with Will + user/pass)
  |
  v
  EVENT LOOP: while (!s_quit) mg_mgr_poll(1000)
  |
  |-[CONNACK rc=0]---> MG_EV_MQTT_OPEN
  |                       |-> mg_mqtt_sub x 3 (Request / Signaling / Status,
  |                           mirroring the 3 topics used by the reference camera source)
  |
  |-[any packet]-----> MG_EV_MQTT_CMD
  |                       |-> log cmd + id
  |
  |-[incoming PUBLISH]-> MG_EV_MQTT_MSG
  |                        |-> log topic + payload
  |                        |-> if payload == "STOP_SUB" -> mg_mqtt_unsub(that topic)
  |
  |-[connection closed]-> MG_EV_CLOSE
  |                        |-> s_reconnect_at = mg_millis() + 60000
  |                        (next poll iteration, main loop checks the timer -> calls mg_mqtt_connect again)
  |
  |-[Ctrl+C]---------> s_quit = 1 -> exit loop
                          |
                          |-> mg_mqtt_disconnect (send DISCONNECT cleanly)
                          |-> mg_mgr_poll x 5 (flush bytes onto the wire)
                          |-> mg_mgr_free
                          EXIT
```

---

## VIII. Build and Run

Requirements: `gcc`, `mbedTLS` development headers (`libmbedtls-dev`), and a `mosquitto` broker (listening on port 1883 with `allow_anonymous true`).

```bash
make            # build -> produces ./mqtt
./mqtt          # run
make clean      # remove the binary
```

Verify the broker:
```bash
systemctl is-active mosquitto       # -> "active"
mosquitto_pub -h 127.0.0.1 -t test -m hi
```

---

## IX. Test Cases

> **Note:** All test cases run on an Ubuntu host against a local Mosquitto broker. Nothing has been ported to a board yet.

### Test 1 - Subscribe, Receive, Unsubscribe on Command

**Purpose:** Verify the standard pub/sub flow between two independent clients, and demonstrate the client unsubscribing itself when the broker delivers a special command payload (`STOP_SUB`).

**Code references:**
- [`mqtt.c` L23-L41](../application/sources/app/mqtt/mqtt_client/mqtt.c#L23-L41) - `MG_EV_MQTT_OPEN` handler: subscribe to the three topics `Request`, `Signaling`, `Status` (one `mg_mqtt_sub` call each).
- [`mqtt.c` L76-L103](../application/sources/app/mqtt/mqtt_client/mqtt.c#L76-L103) - `MG_EV_MQTT_MSG` handler: log the payload and call `mg_mqtt_unsub` when the payload equals `"STOP_SUB"`.

**Setup:** two terminals.

**Terminal A - subscriber (our client):**
```bash
./mqtt
```

**Terminal B - publisher (`mosquitto_pub` sending messages):**
```bash
mosquitto_pub -h 127.0.0.1 -t Request -m "send packet 1"
mosquitto_pub -h 127.0.0.1 -t Request -m "send packet 2"
mosquitto_pub -h 127.0.0.1 -t Request -m "STOP_SUB"
mosquitto_pub -h 127.0.0.1 -t Request -m "check after stop"
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_subcribe_publish_unsubcribe.png" alt="mqtt_subscribe_publish_unsubscribe" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 1:</em></strong> Subscribe, publish, and unsubscribe</p>

**Reading the evidence (Figure 1):**

Terminal A (left) - `./mqtt` output:
```
CONNACK rc=0
CMD cmd=2 id=0                                  <- CONNACK echoed through MG_EV_MQTT_CMD
CMD cmd=9 id=1                                  <- SUBACK for 'Request'
CMD cmd=9 id=2                                  <- SUBACK for 'Signaling'
CMD cmd=9 id=3                                  <- SUBACK for 'Status'
RECV topic='Request' payload='send packet 1'
CMD cmd=3 id=0                                  <- incoming PUBLISH (QoS 0, id=0)
RECV topic='Request' payload='send packet 2'
CMD cmd=3 id=0
RECV topic='Request' payload='STOP_SUB'
CMD cmd=3 id=0
CMD cmd=11 id=4                                 <- UNSUBACK ('Request' unsubscribed)
```

The final message (`check after stop`) does **not** appear in Terminal A, exactly as expected: the client sent UNSUBSCRIBE for `Request` as soon as it processed `STOP_SUB`.

**Observations:**
- Every incoming MQTT packet fires **both** events: `MG_EV_MQTT_CMD` (for any packet, produces `CMD cmd=...` lines) **and** `MG_EV_MQTT_MSG` (for PUBLISH only, produces `RECV topic=... payload=...` lines). That is why each PUBLISH produces two consecutive log lines.
- `id=0` on the PUBLISH packets happens because `mosquitto_pub` defaults to QoS 0 (no packet identifier).
- Subscribing to three topics requires **three separate** `mg_mqtt_sub()` calls: each Mongoose SUBSCRIBE packet carries exactly one topic.
- `mg_mqtt_unsub()` needs only one call; the broker returns UNSUBACK immediately (id=4, right after the initial three SUBACKs).

---

### Test 2 - Last Will Fires on Abrupt Disconnect

**Purpose:** Verify the Last Will and Testament (LWT) mechanism: when the client dies unexpectedly (power loss, crash, network drop), the broker automatically publishes the "will" message on the Will topic so other clients can react. `pkill -9 mqtt` simulates a sudden crash (SIGKILL cannot be caught, so the signal handler never runs).

**Code references:**
- [`mqtt.c` L130-L132](../application/sources/app/mqtt/mqtt_client/mqtt.c#L130-L132) - Will fields in `s_opts`: `topic` = `demo/mqtt/will`, `message` = `client's disconnected`, `qos` = 1.
- [`mqtt.c` L138](../application/sources/app/mqtt/mqtt_client/mqtt.c#L138) - `mg_mqtt_connect` call with `&s_opts` so Mongoose encodes the Will into the CONNECT packet.

**Setup:** three terminals.

**Terminal A - Will watcher:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```
(Wildcard `#` shows every topic under `demo/mqtt/`.)

**Terminal B - run the client:**
```bash
./mqtt
```

**Terminal C - kill -9 (simulated crash):**
```bash
pkill -9 mqtt
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_last_will.png" alt="mqtt_last_will" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 2:</em></strong> Last Will and Testament (LWT)</p>

**Reading the evidence (Figure 2):**

- Terminal A (left): prints exactly one line, `demo/mqtt/will client's disconnected`, right after the process is killed.
- Terminal B (middle): shows `CONNACK rc=0` and three SUBACK lines (`CMD cmd=9 id=1/2/3`), then the shell prints `Killed`. The process was terminated by SIGKILL from outside; there is **no** `CLOSE` line because SIGKILL cannot be caught, the signal handler never ran, and the app had no chance to log anything further.
- Terminal C (right): just the `pkill -9 mqtt` command.

**Broker logic:** the broker sees the TCP socket half-close (FIN/RST) without having received a proper DISCONNECT packet, treats the client as crashed, and fires the Will message on the topic that was registered in CONNECT.

**Meaning:**
- `opts.topic` + `opts.message` in the CONNECT options are the **Will topic** and **Will payload**, not a regular data topic to publish on.
- `opts.qos` in `mg_mqtt_opts` at `mg_mqtt_connect()` time is actually the **Will QoS** (bits 3 and 4 of the Connect Flags byte).
- To trigger the Will, `kill -9` (SIGKILL) is **required**. Other exit paths (Ctrl+C -> SIGINT, `kill` -> SIGTERM, `pkill` without `-9`) reach the signal handler, the app sends DISCONNECT first, and the broker treats the exit as intentional, so the Will does not fire (see Test 3).

---

### Test 3 - Graceful Disconnect: Will Does Not Fire

**Purpose:** Verify that when the client sends a proper MQTT DISCONNECT packet, the broker does **not** fire the Will. This is how a well-behaved application should shut down.

**Code references:**
- [`mqtt.c` L9-L14](../application/sources/app/mqtt/mqtt_client/mqtt.c#L9-L14) - `s_quit` flag and the `on_sigint` signal handler that sets it.
- [`mqtt.c` L134-L135](../application/sources/app/mqtt/mqtt_client/mqtt.c#L134-L135) - handler registration for `SIGINT` and `SIGTERM`.
- [`mqtt.c` L159-L167](../application/sources/app/mqtt/mqtt_client/mqtt.c#L159-L167) - after the main loop exits: call `mg_mqtt_disconnect`, then poll 5 times x 100ms to flush bytes to the socket before `mg_mgr_free`.

**Setup:** two terminals.

**Terminal A - Will watcher:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```

**Terminal B - run the client and press Ctrl+C:**
```bash
./mqtt
# (wait a few seconds for CONNACK + 3 SUBACKs)
# Press Ctrl+C
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_gratefully_disconnecting.png" alt="mqtt_gracefully_disconnecting" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 3:</em></strong> Graceful disconnect</p>

**Reading the evidence (Figure 3):**

- Terminal A (left): only the `mosquitto_sub` command line; no output at all -> the Will was **not** published by the broker.
- Terminal B (right) - `./mqtt` output:
  ```
  CONNACK rc=0
  CMD cmd=2 id=0          <- CONNACK echoed through MG_EV_MQTT_CMD
  CMD cmd=9 id=1          <- SUBACK 'Request'
  CMD cmd=9 id=2          <- SUBACK 'Signaling'
  CMD cmd=9 id=3          <- SUBACK 'Status'
  ^C                      <- Ctrl+C
  CLOSE                   <- MG_EV_CLOSE fires after mg_mqtt_disconnect()
                          -> mg_mgr_free closes the socket
  ```

**Meaning:**
- Mongoose does **not** trap signals itself; that is the job of libc via `<signal.h>`. The canonical pattern is:
  1. `signal(SIGINT, handler)` registers the handler.
  2. The handler sets `s_quit = 1` (volatile so the compiler does not optimize it away).
  3. The main loop checks `!s_quit` and exits when the flag is set.
  4. Call `mg_mqtt_disconnect()` to send a proper DISCONNECT packet.
  5. Poll a few more iterations (five iterations x 100ms here) to **flush** the buffered bytes to the socket.
  6. `mg_mgr_free()` closes and releases everything.

- The flush step in (5) is critical: `mg_mqtt_disconnect()` only **pushes 2 bytes (`0xE0 0x00`) into the connection's send buffer**; the bytes are not yet on the wire. `mg_mgr_poll()` needs to run a few more times so that epoll grants write readiness and the kernel `send()` actually goes out.

---

### Test 4 - Auto-reconnect khi mất kết nối

**Mục đích:** Verify client tự kết nối lại khi broker bị tắt rồi bật lại. Sự cố mạng/restart broker là chuyện bình thường, app phải sống được qua đó.

**Code tham chiếu:**
- [`mqtt.c` L4](../application/sources/app/mqtt/mqtt_client/mqtt.c#L4) — macro `RECONNECT_MS = 60000` (1 phút).
- [`mqtt.c` L104-L114](../application/sources/app/mqtt/mqtt_client/mqtt.c#L104-L114) — handler `MG_EV_CLOSE`: set `s_reconnect_at = mg_millis() + RECONNECT_MS` khi connection rớt.
- [`mqtt.c` L146-L156](../application/sources/app/mqtt/mqtt_client/mqtt.c#L146-L156) — main loop check `s_reconnect_at` mỗi vòng poll, đến giờ thì gọi lại `mg_mqtt_connect`.
- [`mqtt.c` L23-L41](../application/sources/app/mqtt/mqtt_client/mqtt.c#L23-L41) — sub 3 topic nằm trong `MG_EV_MQTT_OPEN` nên tự re-sub sau mỗi lần reconnect, không cần code thêm.

**Setup:** 2 terminal. Em dùng luôn broker chính ở `127.0.0.1:1883` qua `systemd`, `stop` rồi `start` để mô phỏng broker chết tạm thời.

Em đặt `RECONNECT_MS = 60000` (1 phút) trong code. Trên board có thể chỉnh lên tùy yêu cầu; em để 1 phút khi test cho đỡ phải chờ lâu, sau này em sẽ test kĩ hơn khi port lên board.

**Terminal A — chạy client:**
```bash
./mqtt
```
Đợi `CONNACK rc=0` + 3 dòng `CMD cmd=9` (sub xong 3 topic) là OK.

**Terminal B — stop broker, đợi, rồi start lại:**
```bash
sudo systemctl stop mosquitto && sleep 90 && sudo systemctl start mosquitto
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_auto_reconnect.png" alt="mqtt_auto_reconnect" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 4:</em></strong> Auto-reconnect</p>

**Đọc evidence (Hình 4), 3 giai đoạn rõ ràng:**

```
[Giai đoạn 1 — kết nối lần đầu, broker còn sống]
CONNACK rc=0
CMD cmd=2 id=0
CMD cmd=9 id=1                         ← SUBACK 'Request'
CMD cmd=9 id=2                         ← SUBACK 'Signaling'
CMD cmd=9 id=3                         ← SUBACK 'Status'

[Giai đoạn 2 — broker bị stop, TCP rớt]
CLOSE                                  ← MG_EV_CLOSE fire ngay khi mất TCP
Will auto-reconnect in 60000 ms        ← timer reconnect được set

[Giai đoạn 2.5 — sau 60s, broker vẫn chưa lên → connect fail]
mongoose.c:1934:mg_error 2 4 socket error   ← Mongoose log nội bộ: connect bị refused
CLOSE                                  ← MG_EV_CLOSE fire lần nữa
Will auto-reconnect in 60000 ms        ← timer reset, đợi tiếp 60s

[Giai đoạn 3 — sau 60s nữa, broker đã start lại → reconnect OK]
CONNACK rc=0
CMD cmd=2 id=0
CMD cmd=9 id=4                         ← SUBACK 'Request' (lần 2)
CMD cmd=9 id=5                         ← SUBACK 'Signaling'
CMD cmd=9 id=6                         ← SUBACK 'Status'
```

**Quan sát quan trọng:**
- Packet ID tăng dần (1→2→3 ở lần đầu, rồi 4→5→6 ở lần reconnect) — Mongoose dùng 1 counter chung cho cả mgr nên không reset khi connection mới.
- Dòng `mongoose.c:1934:mg_error 2 4 socket error` là log mặc định của Mongoose khi `connect()` syscall thất bại; "2 4" là `(fd, errno)` từ debug nội bộ.

**Kết luận:**
- Mongoose **không có** auto-reconnect sẵn nên phải tự code: trong `MG_EV_CLOSE` set `s_reconnect_at = mg_millis() + RECONNECT_MS`, main loop check `mg_millis() >= s_reconnect_at` thì gọi lại `mg_mqtt_connect()`.
- Một điểm hay: code sub 3 topic nằm trong handler `MG_EV_MQTT_OPEN`, mà event này fire mỗi lần CONNACK về (kể cả sau reconnect). Vậy nên sau khi reconnect, 3 topic tự sub lại không cần thêm code — đó là lý do em không tự gọi lại `mg_mqtt_sub()` từ main loop.
- Để gọi được `mg_mqtt_connect()` từ main loop (lúc reconnect), em phải đưa `mgr`, `opts` ra ngoài thành biến `static` global, không thể để local trong `main()` như phiên bản đầu tiên.

---

### Test 5 — Username / Password authentication

**Mục đích:** Em set `opts.user = "ctp"` và `opts.pass = "aloalo"` trong code, nhưng broker mặc định ở port `1883` đang `allow_anonymous true` — kiểu gì cũng accept nên không thấy được tác dụng của auth. Em dựng thêm 1 broker riêng ở port `1884` có bật `allow_anonymous false` + `password_file` để verify thật cả 2 chiều: pass đúng → OK, pass sai → reject.

**Code tham chiếu:**
- [`mqtt.c` L128-L129](../application/sources/app/mqtt/mqtt_client/mqtt.c#L128-L129) — set `s_opts.user = "ctp"` và `s_opts.pass = "aloalo"` để Mongoose encode vào gói CONNECT (bật cờ User Name Flag + Password Flag).
- [`mqtt.c` L25](../application/sources/app/mqtt/mqtt_client/mqtt.c#L25) — log CONNACK return code trong `MG_EV_MQTT_OPEN` (rc=0 = accepted, rc=5 = not authorized).

**Setup broker auth (1 lần):**

```bash
# Tạo file password — user = ctp, pass = aloalo (file lưu ở /tmp/mosq_pw)
mosquitto_passwd -c -b /tmp/mosq_pw ctp aloalo

# Chạy broker auth ở port 1884 (terminal riêng, background)
mosquitto -c mosq_auth.conf &
```

Nội dung `mosq_auth.conf`:
```
listener 1884
allow_anonymous false
password_file /tmp/mosq_pw
persistence false
```

#### Test 5a — Verify broker auth: pass đúng

Trước khi đụng đến `./mqtt`, em verify broker auth hoạt động đúng bằng `mosquitto_pub`:

```bash
mosquitto_pub -h 127.0.0.1 -p 1884 -u ctp -P aloalo -t x -m hello
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_correct_pass.png" alt="mqtt_correct_pass" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 5a:</em></strong> Broker chấp nhận client với password đúng</p>

**Đọc evidence (Hình 5a):**

- Terminal trái — broker log (`mosquitto -c mosq_auth.conf`):
  ```
  mosquitto version 2.0.11 starting
  Config loaded from mosq_auth.conf.
  Opening ipv4 listen socket on port 1884.
  Opening ipv6 listen socket on port 1884.
  mosquitto version 2.0.11 running
  New connection from 127.0.0.1:59374 on port 1884.
  New client connected from 127.0.0.1:59374 as auto-188BEE5D-... (p2, c1, k60, u'ctp').
  Client auto-188BEE5D-... disconnected.
  ```
  → Dòng `u'ctp'` cho thấy broker đã đọc được username từ gói CONNECT và validate password thành công.

- Terminal phải — lệnh client + kết quả: `mosquitto_pub -h 127.0.0.1 -p 1884 -u ctp -P aloalo -t x -m hello` thoát ngay, không có error → broker accept.

Khi đổi URL trong `mqtt.c` thành `mqtt://127.0.0.1:1884` và build lại, `./mqtt` sẽ in `CONNACK rc=0` rồi 3 dòng `CMD cmd=9` y như Test 1 — broker ứng xử với 2 client (`mosquitto_pub` và `./mqtt`) hoàn toàn giống nhau khi credential hợp lệ.

#### Test 5b — Verify broker auth: pass sai

```bash
mosquitto_pub -h 127.0.0.1 -p 1884 -u ctp -P olaola -t x -m hello
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_incorrect_pass.png" alt="mqtt_incorrect_pass" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 5b:</em></strong> Broker reject client với password sai</p>

**Đọc evidence (Hình 5b):**

- Terminal trái — broker log có thêm 2 dòng cuối so với Hình 5a:
  ```
  New connection from 127.0.0.1:45550 on port 1884.
  Client <unknown> disconnected, not authorised.
  ```
  Lưu ý `Client <unknown>` — broker reject **trước khi** chấp nhận username, nên không log `u'ctp'`.

- Terminal phải — output `mosquitto_pub`:
  ```
  Connection error: Connection Refused: not authorised.
  Error: The connection was refused.
  ```

Khi đổi `opts.pass = mg_str("olaola")` trong `mqtt.c` và build lại, `./mqtt` sẽ nhận CONNACK với return code = 5 (= "not authorized" theo MQTT 3.1.1 spec). Trong code của em, `MG_EV_MQTT_OPEN` sẽ log `CONNACK rc=5`, sau đó Mongoose tự set `c->is_closing = 1` ở [mongoose.c:6877](../application/sources/app/mqtt/mqtt_client/lib/mongoose.c#L6877) → fire `MG_EV_CLOSE` → auto-reconnect logic vẫn kích hoạt nhưng vô ích vì pass vẫn sai → loop mãi cho đến khi user can thiệp.

**Ý nghĩa Test 5:**
- `opts.user` + `opts.pass` được Mongoose encode vào gói CONNECT: set 2 bit "User Name Flag" và "Password Flag" trong Connect Flags byte, đồng thời append 2 chuỗi (length-prefixed) vào payload.
- CONNACK return code theo MQTT 3.1.1 spec:
  - `0` = accepted
  - `1` = unacceptable protocol version
  - `2` = identifier rejected (client_id không hợp lệ)
  - `3` = server unavailable
  - `4` = bad user name or password (format sai)
  - `5` = not authorized (đúng format nhưng broker không cho phép)
- Broker Mosquitto 2.x dùng `rc=5` cho cả "không đúng pass" lẫn "user không tồn tại" — không phân biệt, để tránh information leak.

**Restore sau test:** đổi lại URL `1883` và `opts.pass = "aloalo"`, kill broker auth bằng `pkill -f mosq_auth`.

---

### Test 6 — Publish: client tự gửi message lên broker

**Mục đích:** Verify client thực sự publish được message lên broker qua `mg_mqtt_pub()`. Em demo 2 case publish trong code:
1. Lúc CONNACK về, pub `{"status":"online"}` lên topic `Status` với `retain=true` — báo cho cloud biết camera đã online.
2. Mỗi khi nhận msg trên topic `Request`, pub `{"status":"ok"}` lên topic `Response` — giả lập việc trả lời command từ cloud.

**Code tham chiếu:**
- [`mqtt.c` L43-L50](../application/sources/app/mqtt/mqtt_client/mqtt.c#L43-L50) — pub status `online` cuối handler `MG_EV_MQTT_OPEN`.
- [`mqtt.c` L83-L91](../application/sources/app/mqtt/mqtt_client/mqtt.c#L83-L91) — pub response trong handler `MG_EV_MQTT_MSG` khi nhận msg trên `Request`.

**Setup:** 4 terminal.

**Terminal 1 — sub `Status` từ ngoài để xem msg `online`:**
```bash
mosquitto_sub -h 127.0.0.1 -t Status -v
```

**Terminal 2 — sub `Response` từ ngoài để xem response khi gửi Request:**
```bash
mosquitto_sub -h 127.0.0.1 -t Response -v
```

**Terminal 3 — chạy client:**
```bash
./mqtt
```

**Terminal 4 — gửi 2 cmd lên `Request`:**
```bash
mosquitto_pub -h 127.0.0.1 -t Request -m "do something"
mosquitto_pub -h 127.0.0.1 -t Request -m "more and more"
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_publish.png" alt="mqtt_publish" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 6:</em></strong> Publish</p>

**Đọc evidence (Hình 6):**

Terminal 1 (trái) — `mosquitto_sub -t Status`: in 2 dòng `Status {"status":"online"}`. Dòng đầu là **retained msg** broker delivery ngay khi sub mới connect (do lần test trước đã pub Status với `retain=true` nên broker lưu lại); dòng thứ 2 là msg `./mqtt` của lần test hiện tại pub khi nhận CONNACK.

Terminal 2 (trái-giữa) — `mosquitto_sub -t Response`: in 2 dòng `Response {"status":"ok"}`, mỗi dòng tương ứng với 1 cmd `Request` từ Terminal 4.

Terminal 3 (giữa-phải) — output `./mqtt` (3 dòng SUBACK đầu đã scroll khỏi vùng nhìn thấy):
```
CMD cmd=3 id=2                              ← PUBLISH broker forward về (Status retain loop-back)
CMD cmd=4 id=4                              ← PUBACK broker trả cho msg Status mà client tự pub (id=4)
RECV topic='Request' payload='do something' ← msg từ Terminal 4
CMD cmd=3 id=0                              ← PUBLISH event cho Request (mosquitto_pub mặc định QoS 0 → id=0)
CMD cmd=4 id=5                              ← PUBACK cho msg Response client vừa pub (id=5)
RECV topic='Request' payload='more and more'
CMD cmd=3 id=0
CMD cmd=4 id=6                              ← PUBACK cho msg Response thứ 2 (id=6)
```

Terminal 4 (phải) — publisher: 2 lệnh `mosquitto_pub -t Request -m "do something"` và `-m "more and more"`.

**Quan sát:**
- `mg_mqtt_pub()` chỉ cần 1 lệnh — broker trả PUBACK (`cmd=4`) vì client pub với QoS 1. Có thể thấy 3 PUBACK liên tiếp: `id=4` cho Status `online`, `id=5` và `id=6` cho 2 Response. Packet ID tăng monotonic từ counter chung của Mongoose (sau 3 SUBACK `id=1/2/3` ban đầu là PUBLISH `id=4` của Status, rồi 2 PUBLISH `id=5/6` của Response).
- `retain=true` trên Status pub có nghĩa broker lưu lại msg này. Subscriber **kết nối sau** vẫn nhận được — đó là lý do Terminal 1 in được dòng `online` ngay khi vừa sub xong (chưa cần `./mqtt` pub gì lúc đó).
- Vì client cũng sub topic `Status` mà chính nó pub, broker forward msg về client (loop-back) — đây là đặc tính mặc định của Mosquitto. Dòng `CMD cmd=3 id=2` ở đầu screenshot chính là gói PUBLISH broker đẩy về (broker tự assign packet id cho downlink, không trùng counter của client).

---

### Test 7 — TLS: kết nối client tới broker qua `mqtts://` với self-signed CA

**Mục đích:** Nâng cấp kết nối từ plain TCP (`mqtt://`) lên TLS (`mqtts://`) để mã hóa dữ liệu giữa client và broker. Em verify: (a) handshake TLS thành công với CA đúng → pub/sub hoạt động bình thường, (b) handshake fail khi CA sai → connection bị reject ngay từ giai đoạn TLS.

**Code tham chiếu:**
- [`mqtt.c` L5-L6](../application/sources/app/mqtt/mqtt_client/mqtt.c#L5-L6) — đổi URL sang `mqtts://localhost:8883` và `CA_CERT_PATH = "certs/ca.crt"` (CA tự ký của broker local, không dùng system trust store vì mbedtls không nuốt nổi bundle 220KB với 147 cert mà các build mặc định không support hết thuật toán).
- [`mqtt.c` L52-L70](../application/sources/app/mqtt/mqtt_client/mqtt.c#L52-L70) — handler `MG_EV_CONNECT`: đọc CA bằng `mg_file_read`, gọi `mg_tls_init(c, &opts)` với `opts.ca` = nội dung CA + `opts.name` = host từ URL (dùng cho SNI và CN/SAN verification).
- [`Makefile` L2-L3](../application/sources/app/mqtt/mqtt_client/Makefile#L2-L3) — bật TLS backend mbedtls: `-DMG_TLS=MG_TLS_MBED` + link `-lmbedtls -lmbedx509 -lmbedcrypto`.

#### Phase 1 — Setup CA + cert + broker (1 lần)

Em để artifact TLS trong folder `certs/` cạnh `mqtt.c`:

```bash
mkdir -p certs && cd certs

# CA tự ký
openssl genrsa -out ca.key 2048
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
  -subj "/C=VN/ST=HCM/O=CTP-Test/CN=ctp-test-ca"

# Server cert ký bởi CA, SAN gồm cả IP và DNS
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/C=VN/ST=HCM/O=CTP-Test/CN=127.0.0.1"
cat > server.ext <<'EOF'
subjectAltName = @alt_names
[alt_names]
IP.1  = 127.0.0.1
DNS.1 = localhost
EOF
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 3650 -extfile server.ext

cd ..
```

Broker config (`mosq_tls.conf`, cạnh `mqtt.c`):

```
listener 8883
allow_anonymous true
cafile   certs/ca.crt
certfile certs/server.crt
keyfile  certs/server.key
persistence false
```

> Note: Folder `certs/` đã được thêm vào `.gitignore`

#### Phase 2 — Test 7a: Pub/Sub qua TLS (positive)

**Setup:** 3 terminal, đều ở thư mục `mqtt_client/`.

**Terminal A — broker TLS:**
```bash
mosquitto -c mosq_tls.conf -v
```

**Terminal B — chạy client TLS:**
```bash
./mqtt
```

**Terminal C — publish qua TLS bằng mosquitto_pub:**
```bash
mosquitto_pub -h localhost -p 8883 --cafile certs/ca.crt -t Request -m "tls hello"
mosquitto_pub -h localhost -p 8883 --cafile certs/ca.crt -t Request -m "STOP_SUB"
mosquitto_pub -h localhost -p 8883 --cafile certs/ca.crt -t Request -m "after stop"
```

**Đọc evidence — log Terminal B (`./mqtt`):**

```
CONNACK rc=0                                          ← TLS handshake xong + MQTT CONNECT accepted
CMD cmd=2 id=0                                        ← CONNACK echo qua MG_EV_MQTT_CMD
CMD cmd=9 id=1                                        ← SUBACK 'Request'
CMD cmd=9 id=2                                        ← SUBACK 'Signaling'
CMD cmd=9 id=3                                        ← SUBACK 'Status'
RECV topic='Status' payload='{"status":"online"}'     ← retained msg về (lần test cũ)
RECV topic='Status' payload='{"status":"online"}'     ← chính client tự pub lúc CONNACK (loop-back)
CMD cmd=4 id=4                                        ← PUBACK cho msg Status mình pub
RECV topic='Request' payload='tls hello'              ← msg từ Terminal C
CMD cmd=4 id=5                                        ← PUBACK cho Response mình tự pub
RECV topic='Request' payload='STOP_SUB'
CMD cmd=4 id=6                                        ← PUBACK cho Response thứ 2
CMD cmd=11 id=7                                       ← UNSUBACK (đã unsub 'Request')
CLOSE                                                 ← Ctrl+C → graceful disconnect
```

Msg `"after stop"` **không** xuất hiện trong Terminal B → đúng kỳ vọng: client đã UNSUBSCRIBE topic `Request` sau khi nhận `STOP_SUB`. Logic Test 1 vẫn hoạt động nguyên vẹn qua kênh TLS.

#### Phase 3 — Test 7b: Wrong CA → handshake fail (negative)

Để chứng minh client thực sự verify cert chứ không phải bypass, em tạm thời sửa `CA_CERT_PATH = "/tmp/wrong-ca.pem"` (file rác không phải PEM hợp lệ), build lại và chạy:

```
mongoose.c:19470:mg_load_cert  cert err 0x2180        ← mbedtls không parse được CA file
mongoose.c:1934:mg_error       1 4 socket error
CLOSE
Will auto-reconnect in 60000 ms                       ← auto-reconnect logic vẫn kích hoạt
```

**Ý nghĩa:** mbedtls fail ngay ở bước **load CA** (error `0x2180` = `MBEDTLS_ERR_PEM_NO_HEADER_FOOTER_PRESENT`) — chưa kịp đi tới handshake. Nếu sửa thành 1 CA hợp lệ nhưng không khớp với cert broker, lỗi sẽ là `X509 - Certificate verification failed` (`-0x2700`) ở giai đoạn handshake — cũng dẫn tới `CLOSE` và reconnect. Cả hai case đều confirm code không skip TLS verification.

**Ý nghĩa Test 7:**
- Nâng cấp `mqtt://` → `mqtts://` về phía code chỉ cần thêm handler `MG_EV_CONNECT` gọi `mg_tls_init()` — Mongoose tự handle phần encrypt/decrypt sau đó, các API `mg_mqtt_pub/sub/unsub` không thay đổi gì.
- `opts.name` trong `mg_tls_opts` được dùng cho **2 việc**: SNI (Server Name Indication trong TLS ClientHello) và verify hostname against CN/SAN của server cert. Phải khớp với cert broker, nếu không sẽ fail như đã nêu ở bẫy IP/DNS phía trên.
- mbedtls strict hơn OpenSSL về thuật toán support và validation rule — pattern an toàn cho embedded là **pin đúng 1 CA** mình tin (file nhỏ, gọn) thay vì load cả system trust store.
- Tất cả Test 1-6 ở trên đã chạy trên plain TCP `mqtt://127.0.0.1:1883`. Để chạy lại các test đó bằng TLS, đổi `MQTT_SERVER_URL` về `mqtts://localhost:8883`, broker chạy `mosq_tls.conf`, và mọi lệnh `mosquitto_pub/sub` phải thêm `-p 8883 --cafile certs/ca.crt`.

---

## X. Conclusion

### Bẫy `opts.qos` trong CONNECT

`opts.qos` khi gọi `mg_mqtt_connect()` thực ra là **Will QoS** (bit 3-4 của Connect Flags byte trong gói CONNECT), không phải QoS để publish/subscribe. Nếu set `qos != 0` mà không có Will (`opts.topic` rỗng), broker sẽ reject CONNECT theo chuẩn MQTT-3.1.2-13.

Đúng cách: QoS cho publish/subscribe truyền **riêng** trong `opts.qos` khi gọi `mg_mqtt_pub()` / `mg_mqtt_sub()`, không phải lúc CONNECT.

### Signal handling là việc của libc, không phải Mongoose

Mongoose không quan tâm signal — app phải tự dùng `signal()` của `<signal.h>` để bắt SIGINT/SIGTERM. Không có handler thì kernel terminate process tức thì, gói DISCONNECT không kịp gửi → broker hiểu là crash → fire Will (giống Test 2).

Pattern chuẩn: set cờ trong handler → main loop check cờ → thoát loop → `mg_mqtt_disconnect()` → poll thêm vài lần để flush → `mg_mgr_free()`.

### Sub nhiều topic phải gọi `mg_mqtt_sub()` nhiều lần

API `mg_mqtt_sub()` mỗi lần gọi gửi 1 gói SUBSCRIBE chứa **1 topic duy nhất**. Muốn sub 3 topic phải gọi 3 lần riêng biệt, broker trả về 3 SUBACK riêng biệt với 3 packet ID khác nhau (như Hình 1 và Hình 4).

### Mongoose không cover sẵn auto-reconnect

Cần code tay theo pattern:
- Trong `MG_EV_CLOSE`: `s_reconnect_at = mg_millis() + RECONNECT_MS`.
- Main loop check `mg_millis() >= s_reconnect_at` → gọi lại `mg_mqtt_connect()`.
- `mgr` + `opts` phải để global static để truy cập được từ main loop.

Bonus: vì sub topic nằm trong handler `MG_EV_MQTT_OPEN` (fire mỗi lần CONNACK), nên sau reconnect 3 topic tự sub lại — không cần code thêm.

---

## XI. References

- Mongoose upstream repository: <https://github.com/cesanta/mongoose>
- Cesanta documentation hub: <https://mongoose.ws/documentation/>
- MQTT Client tutorial: <https://mongoose.ws/documentation/tutorials/mqtt/mqtt-client/>
- MQTT Server tutorial: <https://mongoose.ws/documentation/tutorials/mqtt/mqtt-server/>
