# Embedded Application Stack

Connectivity stack cho ứng dụng embedded — MQTT, HTTP/HTTPS, TLS — build trên thư viện [mongoose](https://github.com/cesanta/mongoose). Mục tiêu cuối: chạy được trên board, hiện đang ở phase study từng giao thức trên Linux trước khi port.

## Trạng thái

| Module | Trạng thái | Báo cáo |
|---|---|---|
| MQTT  | Done study trên Linux (mosquitto local) — sub/pub/unsub, Last Will, graceful disconnect, auto-reconnect, auth (user/pass) | [docs/mqtt-reporting.md](docs/mqtt-reporting.md) |
| HTTP  | TODO | — |
| HTTPS | TODO | — |
| TLS   | TODO | — |

Chưa port lên board thật.

## Cấu trúc repo

```
embedded-application-stack/
├── application/
│   └── sources/
│       └── app/
│           └── <module>/         # mỗi giao thức 1 folder
│               ├── lib/          # thư viện mongoose (mongoose.c, mongoose.h)
│               ├── <module>.c    # code chính
│               └── Makefile      # build với gcc, link mongoose.c
├── docs/
│   └── <module>-reporting.md     # báo cáo study chi tiết của module
└── resources/
    └── images/
        └── <module>/             # screenshot evidence cho báo cáo
```

Chi tiết từng module xem báo cáo tương ứng trong `docs/`.
