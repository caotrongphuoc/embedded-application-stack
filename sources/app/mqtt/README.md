# Báo cáo study MQTT với thư viện Mongoose

## Mục đích

Học cách sử dụng các API MQTT của thư viện mongoose rồi viết lại thành 1 file c, kết nối tới broker (hiện tại em dùng broker mosquitto local do em chưa test board ạ) và thực hiện các func cơ bản như: sub, pub, unsub, disconnect đàng hoàng, Last Will (disconnect đột ngột, crash), auto-reconnect.

Toàn bộ code nằm trong file `mqtt.c` (~110 dòng), thư viện mongoose nằm trong folder `lib`.

## Cấu trúc folder

```
mqtt/
├── lib/
│   ├── mongoose.c     (thư viện gốc)
│   └── mongoose.h     (thư viện gốc)
├── mqtt.c             (code chính)
├── Makefile           (build với gcc, link mongoose.c)
└── README.md          (báo cáo study của em)
```

## Bảng các API mongoose dùng trong code

| API | Vai trò |
|---|---|
| `mg_mgr_init(mgr)` | Khởi tạo event manager (epoll fd, ID counter) |
| `mg_mgr_poll(mgr, ms)` | 1 vòng event loop, chờ tối đa `ms` mili giây |
| `mg_mgr_free(mgr)` | Đóng mọi connection còn lại + giải phóng |
| `mg_mqtt_connect(mgr, url, opts, fn, fn_data)` | Mở TCP + gửi gói CONNECT |
| `mg_mqtt_sub(c, opts)` | Gửi gói SUBSCRIBE |
| `mg_mqtt_unsub(c, opts)` | Gửi gói UNSUBSCRIBE |
| `mg_mqtt_disconnect(c, opts)` | Gửi gói DISCONNECT (2 byte gồm 0xE0 và 0x00) |
| `mg_str("…")` | Tạo `struct mg_str` gồm có *buf (chuỗi dữ liệu) và len (kích thước) |
| `mg_strcmp(s1, s2)` | So sánh 2 chuỗi `mg_str` |
| `mg_millis()` | Lấy thời gian hiện tại|
| `mg_log_set(level)` | Set log level (`MG_LL_ERROR/INFO/DEBUG/VERBOSE`) |
| `MG_INFO((...))` | Macro log mức INFO |

Tất cả khai báo trong [lib/mongoose.h](lib/mongoose.h), implement trong [lib/mongoose.c](lib/mongoose.c).

## Bảng các event xử lý trong callback `fn`

```c
static void fn(struct mg_connection *c, int ev, void *ev_data);
```

| Event | Khi nào fire | `ev_data` |
|---|---|---|
| `MG_EV_MQTT_OPEN` | Nhận CONNACK | `int *` = return code (0 = OK) |
| `MG_EV_MQTT_CMD` | Nhận bất kỳ gói MQTT nào | `struct mg_mqtt_message *` |
| `MG_EV_MQTT_MSG` | Nhận PUBLISH (có msg tới subscription) | `struct mg_mqtt_message *` |
| `MG_EV_CLOSE` | Connection đóng | `NULL` |

## Bảng mã `cmd` xuất hiện trong log

Khi nhận `MG_EV_MQTT_CMD`, field `m->cmd` cho biết loại gói:

| `cmd` | Tên gói MQTT | Ý nghĩa |
|---|---|---|
| 2 | CONNACK | Broker xác nhận connect |
| 3 | PUBLISH | Có msg tới topic mình sub |
| 4 | PUBACK | Broker ack msg QoS≥1 mình vừa pub |
| 9 | SUBACK | Broker ack gói SUBSCRIBE |
| 11 | UNSUBACK | Broker ack gói UNSUBSCRIBE |
| 13 | PINGRESP | Broker trả lời keep-alive PING |

---

## Cấu hình trong code

Phần này em hard-code các tham số trực tiếp để thuận tiện cho việc demo:

| Tham số | Giá trị | Vị trí |
|---|---|---|
| Broker URL | `mqtt://127.0.0.1:1883` | Trong dòng `mg_mqtt_connect` |
| Client ID | `client` | `opts.client_id` |
| Username | `ctp` | `opts.user` |
| Password | `aloalo` | `opts.pass` |
| Keep-alive | 60 giây | `opts.keepalive` |
| MQTT version | 4 (= MQTT 3.1.1) | `opts.version` |
| Clean session | `true` | `opts.clean` |
| Will topic | `demo/mqtt/will` | `opts.topic` |
| Will message | `client's disconnected` | `opts.message` |
| Will QoS | 1 | `opts.qos` |
| Topic sub | `Request`, `Signaling`, `Status` | Trong handler `MG_EV_MQTT_OPEN` |
| Reconnect delay | 60000 ms (1 phút) | `RECONNECT_MS` |

---

## Luồng hoạt động

```
START
  │
  ├─► mg_mgr_init                   tạo epoll fd
  │
  ├─► signal(SIGINT/SIGTERM, …)     đăng ký handler bắt Ctrl+C
  │
  ├─► mg_mqtt_connect               mở TCP + gửi CONNECT (kèm Will + user/pass)
  │
  ▼
  EVENT LOOP: while (!s_quit) mg_mgr_poll(1000)
  │
  ├─[CONNACK rc=0]──► MG_EV_MQTT_OPEN
  │                     └─► mg_mqtt_sub × 3 (3 topic: Request/Signaling/Status) (Lí do 3 topic này do em có tham khảo bên source cam, khi connect thì đã sub sẵn 3 topic này)
  │
  ├─[mọi gói]──► MG_EV_MQTT_CMD
  │                └─► log cmd + id
  │
  ├─[PUBLISH tới]──► MG_EV_MQTT_MSG
  │                    └─► log payload
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

Yêu cầu: gcc, mosquitto broker (chạy port 1883 với `allow_anonymous true`).

```bash
make            # build → tạo file ./mqtt
./mqtt          # chạy
make clean      # xóa binary
```

Verify broker:
```bash
systemctl is-active mosquitto       # "active"
mosquitto_pub -h 127.0.0.1 -t test -m hi
```

---

## Các case test (case test dùng mosquitto, chưa port thử trên board)

### Test 1 — Subscribe + nhận message từ ngoài + unsubscribe theo lệnh

**Mục đích:** Verify đúng mô hình pub/sub thực tế giữa 2 client riêng biệt, và demo cơ chế client tự unsub khi nhận lệnh đặc biệt từ broker.

**Setup:** 2 terminal.

**Terminal A — subscriber:**
```bash
./mqtt
```

**Terminal B — publisher:**
```bash
mosquitto_pub -h 127.0.0.1 -t Request -m "send packet 1"
mosquitto_pub -h 127.0.0.1 -t Request -m "send packet 2"
mosquitto_pub -h 127.0.0.1 -t Request -m "STOP_SUB"
mosquitto_pub -h 127.0.0.1 -t Request -m "after stop"
```

**Kết quả mong đợi ở Terminal A:**
```
CONNACK rc=0
CMD cmd=9 id=1, 2, 3                 ← 3 SUBACK cho 3 topic
RECV topic='Request' payload='data 1'
RECV topic='Request' payload='data 2'
RECV topic='Request' payload='STOP_SUB'
CMD cmd=11 ...                       ← UNSUBACK (đã unsub topic Request)
```

Sau khi nhận `STOP_SUB`, msg tiếp theo (`check after stop`) **không** được nhận nữa vì client đã unsub topic `Request`.

<!-- IMAGE_TEST_1 -->

**Ý nghĩa:**
- `mg_mqtt_unsub()` chỉ cần 1 lệnh, broker xử lý ngay lập tức
- Code phải gọi `mg_mqtt_sub()` **3 lần riêng biệt** cho 3 topic (không thể nhồi vào 1 gói SUBSCRIBE qua API của mongoose)

---

### Test 2 — Last Will fire khi client disconnect đột ngột (crash)

**Mục đích:** Verify cơ chế Last Will — khi client chết bất ngờ (mất điện, crash,...), broker tự pub "di chúc" lên topic Will để các client khác biết. Ở đây em dùng pkill -9 mqtt để test case crash đột ngột

**Setup:** 3 terminal.

**Terminal A — watcher Will:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```
(Wildcard `#` để thấy cả topic `demo/mqtt/will`.)

**Terminal B — chạy client:**
```bash
./mqtt
```

**Terminal C — kill -9 (giả lập crash):**
```bash
pkill -9 mqtt
```

**Kết quả mong đợi ở Terminal A:**
```
demo/mqtt/will client's disconnected
```

→ Broker phát hiện TCP rớt mà chưa nhận gói DISCONNECT → fire Will message lên topic `demo/mqtt/will`.

<!-- IMAGE_TEST_2 -->

**Ý nghĩa:**
- `opts.topic` + `opts.message` trong CONNECT options chính là Will (không phải topic data)
- `opts.qos` trong CONNECT thực ra là **Will QoS** (bit 3-4 của Connect Flags byte). Nếu set `qos != 0` mà không có Will topic, broker sẽ reject CONNECT (theo chuẩn MQTT 3.1.2.6)
- Để test Will, **bắt buộc dùng `kill -9`** (SIGKILL). Các cách thoát khác (Ctrl+C, `kill`, `pkill`) sẽ kích hoạt signal handler → gửi DISCONNECT → Will không fire

---

### Test 3 — Disconnect đàng hoàng: Will KHÔNG fire khi thoát đàng hoàng

**Mục đích:** Verify rằng khi client gửi DISCONNECT đúng spec, broker sẽ không fire Will.

**Setup:** 2 terminal.

**Terminal A — watcher Will:**
```bash
mosquitto_sub -h 127.0.0.1 -t 'demo/mqtt/#' -v
```

**Terminal B — chạy client rồi bấm Ctrl+C:**
```bash
./mqtt
# (đợi vài giây)
# Bấm Ctrl+C
```

**Kết quả mong đợi:**
- Terminal B in thêm `CLOSE` rồi exit
- Terminal A **không in gì** — Will không fire

<!-- IMAGE_TEST_3 -->

**Ý nghĩa:**
- Mongoose **không tự bắt signal** — đó là việc của libc qua `<signal.h>`
- Pattern chuẩn: `signal(SIGINT, handler)` → handler set cờ `s_quit = 1` → main loop thoát → gọi `mg_mqtt_disconnect()` → poll thêm vài lần để flush bytes ra mạng → `mg_mgr_free()`
- Phải poll thêm sau khi gọi `mg_mqtt_disconnect()` vì hàm này chỉ **push 2 byte (`0xE0 0x00`) vào send buffer**, chưa thực sự ra mạng. Cần `mg_mgr_poll()` chạy thêm vài lần để  đẩy bytes qua socket

---

### Test 4 — Auto-reconnect khi mất kết nối

**Mục đích:** Em muốn xem client có tự kết nối lại không khi broker bị tắt rồi bật lại. Sự cố mạng là chuyện bình thường trong production, app phải sống được qua đó.

**Setup:** 2 terminal. Em dùng luôn broker chính ở `127.0.0.1:1883` qua systemd, stop rồi start lại để mô phỏng broker chết.

Em đặt `RECONNECT_MS = 60000` (1 phút) trong code. Lúc thật trên cam có thể chỉnh lên 3-5 phút tùy yêu cầu, lúc test em để 1 phút cho đỡ phải đợi lâu nhưng vẫn đủ thực tế.

**Terminal A — chạy client:**
```bash
./mqtt
```

Đợi thấy `CONNACK rc=0` + 3 dòng `CMD cmd=9` (sub xong 3 topic) là OK.

**Terminal B — stop broker, đợi, rồi start lại:**
```bash
sudo systemctl stop mosquitto
# đợi khoảng 1.5 phút để xem client thử reconnect ít nhất 1 lần và fail
sleep 90
sudo systemctl start mosquitto
```

**Kết quả mong đợi ở Terminal A:**
```
[Phiên 1 - bình thường]
CONNACK rc=0
CMD cmd=9 id=1, 2, 3
                                  ← broker bị stop, client thấy CLOSE
CLOSE
Will auto-reconnect in 60000 ms

[Sau 1 phút - broker chưa lên]
Connecting to mqtt://127.0.0.1:1883 ...
socket error                       ← connect refused vì chưa có broker
CLOSE
Will auto-reconnect in 60000 ms

[Sau 1 phút nữa - broker đã start lại]
Connecting to mqtt://127.0.0.1:1883 ...
CONNACK rc=0                        ← reconnect thành công
CMD cmd=9 id=4, 5, 6                ← sub lại 3 topic
```

<!-- IMAGE_TEST_4 -->

**Kết luận:**
- Mongoose **không có** auto-reconnect sẵn nên phải tự code. Cam dùng hệ task/timer riêng để làm việc này, còn bên mongoose thì em dùng cơ chế đơn giản hơn — set `s_reconnect_at = mg_millis() + RECONNECT_MS` trong `MG_EV_CLOSE`, sau đó main loop check thời điểm này mỗi vòng poll, đến giờ thì gọi lại `mg_mqtt_connect()`.
- Tự nhiên có 1 thứ hay: code sub 3 topic nằm trong handler `MG_EV_MQTT_OPEN`, mà event này fire mỗi lần CONNACK về (kể cả sau reconnect). Vậy nên sau khi reconnect xong, 3 topic tự sub lại không cần code thêm.
- Để gọi được `mg_mqtt_connect()` từ main loop (lúc reconnect), em phải đưa `mgr`, `opts` ra ngoài thành biến `static` global, không thể để local trong `main()` như ban đầu.

---

## Bài học rút ra từ toàn bộ quá trình

### Bẫy `opts.qos` trong CONNECT

`opts.qos` khi gọi `mg_mqtt_connect()` thực ra là **Will QoS** (bit 3-4 của Connect Flags byte trong gói CONNECT). Nếu set `qos != 0` mà không có Will (`opts.topic` rỗng), broker reject CONNECT theo chuẩn MQTT-3.1.2-13.

Đúng cách: QoS cho publish/subscribe truyền **riêng** trong `opts.qos` khi gọi `mg_mqtt_pub()`/`mg_mqtt_sub()`, không phải lúc CONNECT.

### Signal handling là việc của libc

Lúc đầu em tưởng mongoose tự lo SIGINT/Ctrl+C. Sau đó test thấy Ctrl+C cũng làm Will fire (giống `kill -9`) thì mới hiểu — mongoose không quan tâm signal, app phải tự dùng `signal()` của `<signal.h>` để bắt. Không có handler thì kernel giết process tức thì, gói DISCONNECT không kịp gửi → broker hiểu là crash → fire Will.

### Sub nhiều topic phải gọi `mg_mqtt_sub()` nhiều lần

API `mg_mqtt_sub()` mỗi lần gọi gửi 1 gói SUBSCRIBE chứa **1 topic** duy nhất. Muốn sub 3 topic phải gọi 3 lần riêng biệt.

### Mongoose không cover sẵn auto-reconnect

Mongoose không có auto-reconnect built-in, cần code:
- Trong `MG_EV_CLOSE` set `s_reconnect_at = mg_millis() + RECONNECT_MS`
- Main loop check `mg_millis() >= s_reconnect_at` thì gọi lại `mg_mqtt_connect()`
- Phải đưa `mgr` + `opts` thành biến global static để truy cập từ main loop được

---

## Tham khảo

- Repo mongoose gốc: https://github.com/cesanta/mongoose
- Tutorial mongoose MQTT: https://mongoose.ws/tutorials/mqtt-client/
- Cesanta blog: https://mongoose.ws/documentation/
