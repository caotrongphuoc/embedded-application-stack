<h1 align="center">HTTP Server Study Report with the Mongoose Library</h1>

Welcome to the HTTP server study report of this repository. The goal is to work through the Mongoose HTTP API on a Linux host, rewrite it into a single self-contained C file, expose a small JSON endpoint, and cover the essential behaviour (listener, routing, replies, graceful shutdown) end-to-end before porting to a real board.

> **Note:** This report is a **work in progress**. The current source builds and runs against `curl` on Linux, but the study test cases (`Test 1..N`) are still being drafted. Sections marked **TODO** will be filled in as each case is executed and the evidence captured.

---

## Table of Contents

- [I. Purpose](#i-purpose)
- [II. Folder Structure](#ii-folder-structure)
- [III. Mongoose API Reference](#iii-mongoose-api-reference)
- [IV. Callback Events](#iv-callback-events)
- [V. In-Code Configuration](#v-in-code-configuration)
- [VI. Program Flow](#vi-program-flow)
- [VII. Build and Run](#vii-build-and-run)
- [VIII. Test Cases](#viii-test-cases)
- [IX. Conclusion](#ix-conclusion)
- [X. References](#x-references)

---

## I. Purpose

Learn how to use the HTTP API of the Mongoose library, rewrite it into a single C file, run it against a plain `curl` client on the same host, and cover the essential features: `listen`, request routing with `mg_match`, JSON replies with `mg_http_reply`, 404 fallback, and graceful shutdown on SIGINT/SIGTERM.

All source code lives in `http.c`. The Mongoose library sits in the `lib` folder.

---

## II. Folder Structure

Relative to the repository root (`embedded-application-stack/`), the HTTP server module lives at:

```
application/sources/app/http/http_server/
├── lib/
│   ├── mongoose.c          # upstream library
│   └── mongoose.h          # upstream library
├── http.c                  # main source
├── Makefile                # gcc build, links mongoose.c
└── .gitignore              # local build artefacts
```

This report (`http-reporting.md`) lives at `docs/` under the repository root.

---

## III. Mongoose API Reference

The "Source Reference" column links directly to the declaration in `mongoose.h` for quick lookup:

| API | Role | Source Reference |
| --- | --- | --- |
| `mg_mgr_init(mgr)` | Initialize the event manager (epoll fd, ID counter) | [h:1805](../application/sources/app/http/http_server/lib/mongoose.h#L1805) |
| `mg_mgr_poll(mgr, ms)` | Run one event-loop iteration, waiting up to `ms` milliseconds | [h:1804](../application/sources/app/http/http_server/lib/mongoose.h#L1804) |
| `mg_mgr_free(mgr)` | Close every remaining connection and free resources | [h:1806](../application/sources/app/http/http_server/lib/mongoose.h#L1806) |
| `mg_http_listen(mgr, url, fn, fn_data)` | Bind an HTTP listener; returns the listening `mg_connection *` | [h:1876](../application/sources/app/http/http_server/lib/mongoose.h#L1876) |
| `mg_http_reply(c, status, headers, fmt, ...)` | Send a full HTTP response with printf-style body | [h:1884](../application/sources/app/http/http_server/lib/mongoose.h#L1884) |
| `mg_match(str, pattern, caps)` | Glob-style pattern match against `mg_str` (used for URI routing) | [h:1208](../application/sources/app/http/http_server/lib/mongoose.h#L1208) |
| `mg_millis()` | Current time in milliseconds since Unix epoch | [h:2943](../application/sources/app/http/http_server/lib/mongoose.h#L2943) |
| `mg_log_set(level)` | Set log level (`MG_LL_ERROR/INFO/DEBUG/VERBOSE`) | [h:1287](../application/sources/app/http/http_server/lib/mongoose.h#L1287) |
| `MG_INFO((...))` | Macro for INFO-level logging | [h:1333](../application/sources/app/http/http_server/lib/mongoose.h#L1333) |

---

## IV. Callback Events

```c
static void ev_handler(struct mg_connection *c, int ev, void *ev_data);
```

| Event | When It Fires | `ev_data` | Reference |
| --- | --- | --- | --- |
| `MG_EV_HTTP_HDRS` | HTTP headers parsed | `struct mg_http_message *` | [h:1702](../application/sources/app/http/http_server/lib/mongoose.h#L1702) |
| `MG_EV_HTTP_MSG` | Full HTTP request received | `struct mg_http_message *` | [h:1703](../application/sources/app/http/http_server/lib/mongoose.h#L1703) |
| `MG_EV_CLOSE` | Connection closed | `NULL` | [h:1701](../application/sources/app/http/http_server/lib/mongoose.h#L1701) |

---

## V. In-Code Configuration

| Parameter | Value | Location |
| --- | --- | --- |
| Listen URL | `http://0.0.0.0:8000` | Macro `HTTP_LISTEN_URL` |
| JSON route | `/api/stats` | `mg_match` inside `ev_handler` |
| 404 fallback | Every other URI | `else` branch of `ev_handler` |

---

## VI. Program Flow

```
START
  |
  |-> mg_mgr_init                   create epoll fd
  |
  |-> signal(SIGINT/SIGTERM, ...)   register handler for Ctrl+C (used to test graceful shutdown)
  |
  |-> mg_http_listen                bind on HTTP_LISTEN_URL
  |
  v
  EVENT LOOP: while (!s_quit) mg_mgr_poll(1000)
  |
  |-[HTTP request in]-> MG_EV_HTTP_MSG
  |                        |-> mg_match(hm->uri, "/api/stats") ?
  |                        |     yes -> mg_http_reply 200 JSON {uptime_ms, status}
  |                        |     no  -> mg_http_reply 404 JSON {error}
  |
  |-[Ctrl+C]---------> s_quit = 1 -> exit loop
                          |
                          |-> mg_mgr_free
                          EXIT
```

---

## VII. Build and Run

Requirements: `gcc` and `curl`.

```bash
make            # build -> produces ./http
./http          # run
make clean      # remove the binary
```

Verify the server:
```bash
curl -s http://127.0.0.1:8000/api/stats
# -> {"uptime_ms":<N>,"status":"ok"}

curl -s http://127.0.0.1:8000/nope
# -> {"error":"not found"}
```

Stop the server with `Ctrl+C`; the shutdown log line `Shutting down` confirms the signal handler ran and `mg_mgr_free` was called cleanly.

---

## VIII. Test Cases

> **Note:** Full test cases (setup, evidence screenshots, and analysis) are still being drafted. The following entries are placeholders.

### Test 1 - GET `/api/stats` Returns JSON

**Status:** TODO - to be documented with screenshot evidence.

### Test 2 - Unknown URI Returns 404 JSON

**Status:** TODO - to be documented with screenshot evidence.

### Test 3 - Graceful Shutdown on SIGINT/SIGTERM

**Status:** TODO - to be documented with log evidence.

### Test 4 - Static File Serving (`mg_http_serve_dir`)

**Status:** TODO - not yet implemented.

### Test 5 - HTTPS with mbedTLS

**Status:** TODO - not yet implemented. Will mirror the MQTT TLS setup (see [`mqtt-reporting.md` Test 7](mqtt-reporting.md#test-7---tls-connecting-via-mqtts-with-a-self-signed-ca)).

---

## IX. Conclusion

**Status:** TODO - to be filled in once the test cases above have been executed.

---

## X. References

- Mongoose upstream repository: <https://github.com/cesanta/mongoose>
- Cesanta documentation hub: <https://mongoose.ws/documentation/>
- HTTP Server tutorial: <https://mongoose.ws/documentation/tutorials/http/http-server/>
- REST server tutorial: <https://mongoose.ws/documentation/tutorials/http/rest-server/>
