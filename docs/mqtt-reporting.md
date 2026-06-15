# Báo cáo study MQTT với thư viện Mongoose

## Mục đích

Học cách sử dụng các API MQTT của thư viện Mongoose rồi viết lại thành 1 file C, kết nối tới broker (hiện tại em dùng broker Mosquitto local do em chưa test trên board) và thực hiện các function cơ bản: `sub`, `pub`, `unsub`, `disconnect` đàng hoàng, Last Will (disconnect đột ngột / crash), auto-reconnect và authentication user/pass.

Toàn bộ code nằm trong file `mqtt.c`, thư viện Mongoose nằm trong folder `lib`.

## Cấu trúc folder

Tính từ root repo (`embedded-application-stack/`), folder MQTT nằm ở:

```
application/sources/app/mqtt/
├── lib/
│   ├── mongoose.c          # thư viện gốc
│   └── mongoose.h          # thư viện gốc
├── mqtt.c                  # code chính
├── Makefile                # build với gcc, link mongoose.c
└── mosq_auth.conf          # config broker mosquitto bật auth — dùng cho Test 5
```

Báo cáo này (file `mqtt-reporting.md`) nằm tại `docs/` ở root repo.

## Bảng các API Mongoose dùng trong code

Cột "Tham khảo source" link thẳng tới dòng khai báo trong `mongoose.h` và dòng implement trong `mongoose.c` để tra cứu nhanh:

| API | Vai trò | Tham khảo source |
|---|---|---|
| `mg_mgr_init(mgr)` | Khởi tạo event manager (epoll fd, ID counter) | [h:1805](../application/sources/app/mqtt/lib/mongoose.h#L1805) · [c:7262](../application/sources/app/mqtt/lib/mongoose.c#L7262) |
| `mg_mgr_poll(mgr, ms)` | 1 vòng event loop, chờ tối đa `ms` milli giây | [h:1804](../application/sources/app/mqtt/lib/mongoose.h#L1804) · [c:13777](../application/sources/app/mqtt/lib/mongoose.c#L13777) |
| `mg_mgr_free(mgr)` | Đóng mọi connection còn lại + giải phóng tài nguyên | [h:1806](../application/sources/app/mqtt/lib/mongoose.h#L1806) · [c:7242](../application/sources/app/mqtt/lib/mongoose.c#L7242) |
| `mg_mqtt_connect(mgr, url, opts, fn, fn_data)` | Mở TCP + gửi gói CONNECT | [h:3088](../application/sources/app/mqtt/lib/mongoose.h#L3088) · [c:6961](../application/sources/app/mqtt/lib/mongoose.c#L6961) |
| `mg_mqtt_pub(c, opts)` | Gửi gói PUBLISH, trả về `uint16_t` packet ID | [h:3094](../application/sources/app/mqtt/lib/mongoose.h#L3094) · [c:6719](../application/sources/app/mqtt/lib/mongoose.c#L6719) |
| `mg_mqtt_sub(c, opts)` | Gửi gói SUBSCRIBE (1 topic mỗi lần gọi) | [h:3095](../application/sources/app/mqtt/lib/mongoose.h#L3095) · [c:6779](../application/sources/app/mqtt/lib/mongoose.c#L6779) |
| `mg_mqtt_unsub(c, opts)` | Gửi gói UNSUBSCRIBE | [h:3096](../application/sources/app/mqtt/lib/mongoose.h#L3096) · [c:6783](../application/sources/app/mqtt/lib/mongoose.c#L6783) |
| `mg_mqtt_disconnect(c, opts)` | Gửi gói DISCONNECT (2 byte: `0xE0 0x00`) | [h:3102](../application/sources/app/mqtt/lib/mongoose.h#L3102) · [c:6944](../application/sources/app/mqtt/lib/mongoose.c#L6944) |
| `mg_str("…")` | Tạo `struct mg_str` gồm `*buf` (chuỗi dữ liệu) và `len` (kích thước). Là macro gọi `mg_str_s()` | [h:1200](../application/sources/app/mqtt/lib/mongoose.h#L1200) · [c:13938](../application/sources/app/mqtt/lib/mongoose.c#L13938) |
| `mg_strcmp(s1, s2)` | So sánh 2 chuỗi `mg_str` | [h:1205](../application/sources/app/mqtt/lib/mongoose.h#L1205) · [c:13977](../application/sources/app/mqtt/lib/mongoose.c#L13977) |
| `mg_millis()` | Lấy thời gian hiện tại (ms từ Unix epoch) | [h:2943](../application/sources/app/mqtt/lib/mongoose.h#L2943) · [c:24783](../application/sources/app/mqtt/lib/mongoose.c#L24783) |
| `mg_log_set(level)` | Set log level (`MG_LL_ERROR/INFO/DEBUG/VERBOSE`). Là macro gán `mg_log_level` | [h:1287](../application/sources/app/mqtt/lib/mongoose.h#L1287) |
| `MG_INFO((...))` | Macro log mức INFO | [h:1333](../application/sources/app/mqtt/lib/mongoose.h#L1333) |

## Bảng các event xử lý trong callback `fn`

```c
static void fn(struct mg_connection *c, int ev, void *ev_data);
```

| Event | Khi nào fire | `ev_data` | Tham khảo |
|---|---|---|---|
| `MG_EV_MQTT_OPEN` | Nhận CONNACK | `int *` = return code (0 = OK) | [h:1709](../application/sources/app/mqtt/lib/mongoose.h#L1709) |
| `MG_EV_MQTT_CMD` | Nhận bất kỳ gói MQTT nào | `struct mg_mqtt_message *` | [h:1707](../application/sources/app/mqtt/lib/mongoose.h#L1707) |
| `MG_EV_MQTT_MSG` | Nhận PUBLISH (msg tới subscription) | `struct mg_mqtt_message *` | [h:1708](../application/sources/app/mqtt/lib/mongoose.h#L1708) |
| `MG_EV_CLOSE` | Connection đóng | `NULL` | [h:1701](../application/sources/app/mqtt/lib/mongoose.h#L1701) |

## Bảng mã `cmd` xuất hiện trong log

Khi nhận `MG_EV_MQTT_CMD`, field `m->cmd` cho biết loại gói:

| `cmd` | Tên gói MQTT | Ý nghĩa | Tham khảo |
|---|---|---|---|
| 2  | CONNACK   | Broker xác nhận connect | [h:2996](../application/sources/app/mqtt/lib/mongoose.h#L2996) |
| 3  | PUBLISH   | Có msg tới topic mình sub | [h:2997](../application/sources/app/mqtt/lib/mongoose.h#L2997) |
| 4  | PUBACK    | Broker ack msg QoS≥1 mình vừa pub | [h:2998](../application/sources/app/mqtt/lib/mongoose.h#L2998) |
| 9  | SUBACK    | Broker ack gói SUBSCRIBE | [h:3003](../application/sources/app/mqtt/lib/mongoose.h#L3003) |
| 11 | UNSUBACK  | Broker ack gói UNSUBSCRIBE | [h:3005](../application/sources/app/mqtt/lib/mongoose.h#L3005) |
| 13 | PINGRESP  | Broker trả lời keep-alive PING | [h:3007](../application/sources/app/mqtt/lib/mongoose.h#L3007) |

---

## Cấu hình trong code

Phần này em hard-code các tham số trực tiếp để thuận tiện cho việc demo:

| Tham số | Giá trị | Vị trí |
|---|---|---|
| Broker URL | `mqtt://127.0.0.1:1883` | Đối số thứ 2 của `mg_mqtt_connect` |
| Client ID | `client` | `opts.client_id` |
| Username | `ctp` | `opts.user` |
| Password | `aloalo` | `opts.pass` |
| Keep-alive | 60 giây | `opts.keepalive` |
| MQTT version | 4 (= MQTT 3.1.1) | `opts.version` |
| Clean session | `true` | `opts.clean` |
| Will topic | `demo/mqtt/will` | `opts.topic` |
| Will message | `client's disconnected` | `opts.message` |
| Will QoS | 1 | `opts.qos` |
| Topic subscribe | `Request`, `Signaling`, `Status` | Trong handler `MG_EV_MQTT_OPEN` |
| Reconnect delay | 60000 ms (1 phút) | Macro `RECONNECT_MS` |

---

## Luồng hoạt động

```
START
  │
  ├─► mg_mgr_init                   tạo epoll fd
  │
  ├─► signal(SIGINT/SIGTERM, …)     đăng ký handler bắt Ctrl+C (em dùng cho mục đích test case disconnect đàng hoàng)
  │
  ├─► mg_mqtt_connect               mở TCP + gửi CONNECT (kèm Will + user/pass)
  │
  ▼
  EVENT LOOP: while (!s_quit) mg_mgr_poll(1000)
  │
  ├─[CONNACK rc=0]──► MG_EV_MQTT_OPEN
  │                     └─► mg_mqtt_sub × 3 (3 topic Request/Signaling/Status,
  │                         tham khảo theo source camera đang dùng sẵn 3 topic này)
  │
  ├─[mọi gói tới]──► MG_EV_MQTT_CMD
  │                    └─► log cmd + id
  │
  ├─[PUBLISH tới]──► MG_EV_MQTT_MSG
  │                    └─► log topic + payload
  │                    └─► nếu payload == "STOP_SUB" → mg_mqtt_unsub(topic đó)
  │
  ├─[Connection đóng]──► MG_EV_CLOSE
  │                       └─► s_reconnect_at = mg_millis() + 60000
  │                       (lần poll sau, main loop check timer → gọi mg_mqtt_connect lại)
  │
  └─[Ctrl+C]──► s_quit = 1 → thoát loop
                 │
                 ├─► mg_mqtt_disconnect (gửi DISCONNECT đàng hoàng)
                 ├─► mg_mgr_poll × 5 (flush bytes ra mạng)
                 └─► mg_mgr_free
                 EXIT
```

---

## Build & Run

Yêu cầu: `gcc`, `mosquitto` broker (chạy port 1883 với `allow_anonymous true`).

```bash
make            # build → tạo file ./mqtt
./mqtt          # chạy
make clean      # xóa binary
```

Verify broker:
```bash
systemctl is-active mosquitto       # → "active"
mosquitto_pub -h 127.0.0.1 -t test -m hi
```

---

## Các case test

> Toàn bộ case test em chạy trên Ubuntu host với Mosquitto local, chưa port lên board.

### Test 1 — Subscribe + nhận message từ ngoài + unsubscribe theo lệnh

**Mục đích:** Verify đúng mô hình pub/sub thực tế giữa 2 client riêng biệt, và demo cơ chế client tự unsub khi nhận lệnh đặc biệt từ broker (`STOP_SUB`).

**Code tham chiếu:**
- [`mqtt.c` L15-L33](../application/sources/app/mqtt/mqtt.c#L15-L33) — handler `MG_EV_MQTT_OPEN`: sub 3 topic `Request` / `Signaling` / `Status` (mỗi topic 1 lần gọi `mg_mqtt_sub`).
- [`mqtt.c` L47-L71](../application/sources/app/mqtt/mqtt.c#L47-L71) — handler `MG_EV_MQTT_MSG`: log payload và gọi `mg_mqtt_unsub` khi payload trùng `"STOP_SUB"`.

**Setup:** 2 terminal.

**Terminal A — subscriber (client của em):**
```bash
./mqtt
```

**Terminal B — publisher (mosquitto_pub gửi msg):**
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
<p align="center"><strong><em>Hình 1:</em></strong> Subscribe, publish và unsubscribe</p>

**Đọc evidence (Hình 1):**

Terminal A (trái) — output `./mqtt`:
```
CONNACK rc=0
CMD cmd=2 id=0                                  ← CONNACK echo qua MG_EV_MQTT_CMD
CMD cmd=9 id=1                                  ← SUBACK cho 'Request'
CMD cmd=9 id=2                                  ← SUBACK cho 'Signaling'
CMD cmd=9 id=3                                  ← SUBACK cho 'Status'
RECV topic='Request' payload='send packet 1'
CMD cmd=3 id=0                                  ← PUBLISH tới (QoS 0, id=0)
RECV topic='Request' payload='send packet 2'
CMD cmd=3 id=0
RECV topic='Request' payload='STOP_SUB'
CMD cmd=3 id=0
CMD cmd=11 id=4                                 ← UNSUBACK (đã unsub 'Request')
```

Sau khi nhận `STOP_SUB`, msg cuối cùng (`check after stop`) **không** xuất hiện trong Terminal A — đúng kỳ vọng, vì client đã gửi UNSUBSCRIBE topic `Request` ngay khi xử lý `STOP_SUB`.

**Quan sát:**
- Mỗi gói MQTT tới đều fire **cả 2** event: `MG_EV_MQTT_CMD` (cho mọi loại gói, dòng `CMD cmd=...`) **và** `MG_EV_MQTT_MSG` (riêng cho PUBLISH, dòng `RECV topic=... payload=...`). Đó là lý do với mỗi PUBLISH ta thấy 2 dòng log liên tiếp.
- `id=0` ở các gói PUBLISH là do `mosquitto_pub` mặc định dùng QoS 0 (gói không có packet identifier).
- Code phải gọi `mg_mqtt_sub()` **3 lần riêng biệt** cho 3 topic, mỗi gói SUBSCRIBE của Mongoose chỉ mang 1 topic.
- `mg_mqtt_unsub()` chỉ cần 1 lệnh, broker trả về UNSUBACK ngay (id=4 ngay sau 3 SUBACK đầu).

---

### Test 2 — Last Will fire khi client disconnect đột ngột (crash)

**Mục đích:** Verify cơ chế Last Will and Testament (LWT) — khi client chết bất ngờ (mất điện, crash, mất mạng), broker tự pub "di chúc" lên topic Will để các client khác biết. Em dùng `pkill -9 mqtt` để giả lập crash đột ngột (SIGKILL không cho process chạy signal handler).

**Code tham chiếu:**
- [`mqtt.c` L95-L97](../application/sources/app/mqtt/mqtt.c#L95-L97) — set Will fields trong `s_opts`: `topic` = `demo/mqtt/will`, `message` = `client's disconnected`, `qos` = 1.
- [`mqtt.c` L103](../application/sources/app/mqtt/mqtt.c#L103) — gọi `mg_mqtt_connect` với `&s_opts` để Mongoose encode Will vào gói CONNECT.

**Setup:** 3 terminal.

**Terminal A — watcher Will:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```
(Wildcard `#` để thấy mọi topic dưới `demo/mqtt/`.)

**Terminal B — chạy client:**
```bash
./mqtt
```

**Terminal C — kill -9 (giả lập crash):**
```bash
pkill -9 mqtt
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_last_will.png" alt="mqtt_last_will" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 2:</em></strong> Last Will and Testament (LWT)</p>

**Đọc evidence (Hình 2):**

- Terminal A (trái): in ra đúng 1 dòng `demo/mqtt/will client's disconnected` ngay sau khi process bị kill.
- Terminal B (giữa): `CONNACK rc=0` + 3 dòng SUBACK (`CMD cmd=9 id=1/2/3`), rồi shell in `Killed` — chứng tỏ process bị SIGKILL từ bên ngoài, **không có** log `CLOSE` (vì SIGKILL không thể chặn, signal handler không chạy, app không kịp in gì thêm).
- Terminal C (phải): chỉ là lệnh `pkill -9 mqtt`.

**Logic broker:** broker thấy TCP socket bị nửa-đóng (FIN/RST) mà chưa nhận gói DISCONNECT đúng nghĩa → mặc định coi như client crash → fire Will message lên topic đã đăng ký trong CONNECT.

**Ý nghĩa:**
- `opts.topic` + `opts.message` trong CONNECT options chính là **Will topic** + **Will payload** (không phải data topic để pub thường).
- `opts.qos` trong `mg_mqtt_opts` lúc gọi `mg_mqtt_connect()` thực ra là **Will QoS** (bit 3-4 của Connect Flags byte). 
- Để trigger được Will, **bắt buộc dùng `kill -9`** (SIGKILL). Các cách thoát khác (Ctrl+C → SIGINT, `kill` → SIGTERM, `pkill` không có `-9`) sẽ kích hoạt signal handler → app gửi DISCONNECT trước → broker hiểu là thoát đàng hoàng → Will không fire (xem Test 3).

---

### Test 3 — Disconnect đàng hoàng: Will KHÔNG fire khi thoát có chủ ý

**Mục đích:** Verify rằng khi client gửi gói DISCONNECT đúng spec MQTT, broker sẽ **không** fire Will — đây là cách app sống đàng hoàng nên ứng xử.

**Code tham chiếu:**
- [`mqtt.c` L6-L7](../application/sources/app/mqtt/mqtt.c#L6-L7) — định nghĩa cờ `s_quit` và signal handler `on_sigint` set cờ.
- [`mqtt.c` L99-L100](../application/sources/app/mqtt/mqtt.c#L99-L100) — đăng ký handler cho `SIGINT` và `SIGTERM`.
- [`mqtt.c` L116-L123](../application/sources/app/mqtt/mqtt.c#L116-L123) — sau khi main loop thoát: gọi `mg_mqtt_disconnect` rồi poll 5 lần × 100ms để flush bytes ra socket trước khi `mg_mgr_free`.

**Setup:** 2 terminal.

**Terminal A — watcher Will:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```

**Terminal B — chạy client rồi bấm Ctrl+C:**
```bash
./mqtt
# (đợi vài giây cho thấy CONNACK + 3 SUBACK)
# Bấm Ctrl+C
```

<table align="center">
  <tr>
    <td align="center"><img src="../resources/images/mqtt/mqtt_gratefully_disconnecting.png" alt="mqtt_gracefully_disconnecting" width="1700"/></td>
  </tr>
</table>
<p align="center"><strong><em>Hình 3:</em></strong> Gracefully disconnecting</p>

**Đọc evidence (Hình 3):**

- Terminal A (trái): chỉ có dòng lệnh `mosquitto_sub`, **hoàn toàn không có** dòng nào in ra → Will không được broker phát.
- Terminal B (phải) — output `./mqtt`:
  ```
  CONNACK rc=0
  CMD cmd=2 id=0          ← CONNACK echo qua MG_EV_MQTT_CMD
  CMD cmd=9 id=1          ← SUBACK 'Request'
  CMD cmd=9 id=2          ← SUBACK 'Signaling'
  CMD cmd=9 id=3          ← SUBACK 'Status'
  ^C                      ← Ctrl+C
  CLOSE                   ← MG_EV_CLOSE fire sau khi mg_mqtt_disconnect()
                          → mg_mgr_free đóng socket
  ```

**Ý nghĩa:**
- Mongoose **không tự bắt signal** — đó là việc của libc qua `<signal.h>`. Pattern chuẩn:
  1. `signal(SIGINT, handler)` đăng ký handler.
  2. Handler set cờ `s_quit = 1` (volatile để compiler không optimize).
  3. Main loop kiểm tra `!s_quit`, thoát khi thấy cờ.
  4. Gọi `mg_mqtt_disconnect()` để gửi gói DISCONNECT đàng hoàng.
  5. Poll thêm vài vòng (em chạy 5 lần × 100ms) để **flush** bytes ra socket.
  6. `mg_mgr_free()` đóng và giải phóng.

- Bước flush ở (5) rất quan trọng: `mg_mqtt_disconnect()` chỉ **push 2 byte (`0xE0 0x00`) vào send buffer của connection**, chưa thực sự ghi ra socket. Cần `mg_mgr_poll()` chạy thêm vài lần để epoll cho phép write và `send()` xuống kernel.

---

### Test 4 — Auto-reconnect khi mất kết nối

**Mục đích:** Verify client tự kết nối lại khi broker bị tắt rồi bật lại. Sự cố mạng/restart broker là chuyện bình thường, app phải sống được qua đó.

**Code tham chiếu:**
- [`mqtt.c` L4](../application/sources/app/mqtt/mqtt.c#L4) — macro `RECONNECT_MS = 60000` (1 phút).
- [`mqtt.c` L72-L80](../application/sources/app/mqtt/mqtt.c#L72-L80) — handler `MG_EV_CLOSE`: set `s_reconnect_at = mg_millis() + RECONNECT_MS` khi connection rớt.
- [`mqtt.c` L106-L114](../application/sources/app/mqtt/mqtt.c#L106-L114) — main loop check `s_reconnect_at` mỗi vòng poll, đến giờ thì gọi lại `mg_mqtt_connect`.
- [`mqtt.c` L15-L33](../application/sources/app/mqtt/mqtt.c#L15-L33) — sub 3 topic nằm trong `MG_EV_MQTT_OPEN` nên tự re-sub sau mỗi lần reconnect, không cần code thêm.

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
- [`mqtt.c` L93-L94](../application/sources/app/mqtt/mqtt.c#L93-L94) — set `s_opts.user = "ctp"` và `s_opts.pass = "aloalo"` để Mongoose encode vào gói CONNECT (bật cờ User Name Flag + Password Flag).
- [`mqtt.c` L16](../application/sources/app/mqtt/mqtt.c#L16) — log CONNACK return code trong `MG_EV_MQTT_OPEN` (rc=0 = accepted, rc=5 = not authorized).

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

Khi đổi `opts.pass = mg_str("olaola")` trong `mqtt.c` và build lại, `./mqtt` sẽ nhận CONNACK với return code = 5 (= "not authorized" theo MQTT 3.1.1 spec). Trong code của em, `MG_EV_MQTT_OPEN` sẽ log `CONNACK rc=5`, sau đó Mongoose tự set `c->is_closing = 1` ở [mongoose.c:6877](../application/sources/app/mqtt/lib/mongoose.c#L6877) → fire `MG_EV_CLOSE` → auto-reconnect logic vẫn kích hoạt nhưng vô ích vì pass vẫn sai → loop mãi cho đến khi user can thiệp.

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
- [`mqtt.c` L34-L41](../application/sources/app/mqtt/mqtt.c#L34-L41) — pub status `online` cuối handler `MG_EV_MQTT_OPEN`.
- [`mqtt.c` L53-L61](../application/sources/app/mqtt/mqtt.c#L53-L61) — pub response trong handler `MG_EV_MQTT_MSG` khi nhận msg trên `Request`.

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

## Kết luận chung

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

## Tham khảo

- Repo Mongoose gốc: <https://github.com/cesanta/mongoose>
- Cesanta documentation hub: <https://mongoose.ws/documentation/>
- MQTT Client: <https://mongoose.ws/documentation/tutorials/mqtt/mqtt-client/>
- MQTT Server: <https://mongoose.ws/documentation/tutorials/mqtt/mqtt-server/>
