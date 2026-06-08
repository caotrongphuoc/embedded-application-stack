# Embedded Application Stack

Connectivity stack cho ứng dụng embedded — MQTT, HTTP/HTTPS, TLS — build trên thư viện [mongoose](https://github.com/cesanta/mongoose). Mục tiêu cuối: chạy được trên board, hiện đang ở phase study từng giao thức trên Linux trước khi port.

## Trạng thái

| Module | Trạng thái | Báo cáo |
|---|---|---|
| MQTT  | Done study trên Linux (mosquitto local) — sub/pub/unsub, Last Will, graceful disconnect, auto-reconnect | [sources/app/mqtt/README.md](sources/app/mqtt/README.md) |
| HTTP  | TODO | — |
| HTTPS | TODO | — |
| TLS   | TODO | — |

Chưa port lên board thật.

## Cấu trúc repo

```
sources/
└── app/
    └── <module>/        mỗi giao thức 1 folder, có README riêng + code + Makefile
        ├── lib/         thư viện mongoose
        ├── <module>.c   code chính
        ├── Makefile
        └── README.md    báo cáo study chi tiết của module
```

Chi tiết từng module xem README bên trong folder tương ứng.
