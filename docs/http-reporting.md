<h1 align="center">HTTP Server Study Report with the Mongoose Library</h1>

Welcome to the HTTP server study report of this repository. The goal is to work through the Mongoose HTTP API on a Linux host, rewrite it into a single self-contained C file, expose a small JSON endpoint, and cover the essential behaviour (listener, routing, replies, graceful shutdown) end-to-end before porting to a real board.

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
  - [Test 1 - GET `/api/stats` Returns JSON](#test-1---get-apistats-returns-json)
  - [Test 2 - Unknown URI Returns 404 JSON](#test-2---unknown-uri-returns-404-json)
  - [Test 3 - Graceful Shutdown on SIGINT/SIGTERM](#test-3---graceful-shutdown-on-sigintsigterm)
- [IX. Conclusion](#ix-conclusion)
- [X. Future Work](#x-future-work)
- [XI. References](#xi-references)

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

`mg_http_reply` uses Mongoose's `mg_snprintf`-style specifiers. The two relevant ones for this module are:
- `%m` paired with `MG_ESC("string")` prints a JSON-safe quoted string.
- `%lu` prints an unsigned long value.

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

Only `MG_EV_HTTP_MSG` is handled in the current source: the server waits for the full request before deciding how to respond, so the earlier `MG_EV_HTTP_HDRS` is unnecessary.

---

## V. In-Code Configuration

| Parameter | Value | Location |
| --- | --- | --- |
| Listen URL | `http://0.0.0.0:8000` | Macro `HTTP_LISTEN_URL` |
| JSON route | `/api/stats` | `mg_match` inside `ev_handler` |
| 404 fallback | Every other URI | `else` branch of `ev_handler` |
| Poll interval | 1000 ms | 2nd argument of `mg_mgr_poll` |

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

Verify the server responds:
```bash
curl -s http://127.0.0.1:8000/api/stats
# -> {"uptime_ms":<N>,"status":"ok"}

curl -s http://127.0.0.1:8000/nope
# -> {"error":"not found"}
```

Stop the server with `Ctrl+C`; the shutdown log line `Shutting down` confirms the signal handler ran and `mg_mgr_free` was called cleanly.

---

## VIII. Test Cases

> **Note:** All test cases run on an Ubuntu host with the server listening on `0.0.0.0:8000`. Nothing has been ported to a board yet.

### Test 1 - GET `/api/stats` Returns JSON

**Purpose:** Verify that Mongoose routes the request to the correct handler, that `mg_http_reply` sends a well-formed HTTP response, and that `MG_ESC` produces JSON-safe payload.

**Code references:**
- [`http.c` L13-L36](../application/sources/app/http/http_server/http.c#L13-L36) - `ev_handler`: match `/api/stats`, reply 200 with `{uptime_ms, status}`.
- [`http.c` L19](../application/sources/app/http/http_server/http.c#L19) - `mg_match` compares the URI against the literal `/api/stats`.
- [`http.c` L22-L26](../application/sources/app/http/http_server/http.c#L22-L26) - `mg_http_reply` uses `%m` + `MG_ESC(...)` for the string values and `%lu` for `mg_millis()`.

**Setup:** two terminals.

**Terminal A - run the server:**
```bash
./http
```

**Terminal B - hit the endpoint with `curl`:**
```bash
curl -i http://127.0.0.1:8000/api/stats
```

<table align="center">
  <tr>
    <td align="center"><em>Screenshot coming soon</em></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 1:</em></strong> GET /api/stats returns JSON</p>

**Reading the evidence:**

Terminal B (`curl -i`) output:
```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 36

{"uptime_ms":3336765,"status":"ok"}
```

Terminal A (server log):
```
Listening on http://0.0.0.0:8000
GET /api/stats
```

**Observations:**
- Status line is `200 OK`; `Content-Type: application/json` and `Content-Length` are added automatically by `mg_http_reply` because they follow the `headers` argument the code passed in.
- `uptime_ms` is the value of `mg_millis()` at the moment the request was handled, so its exact number changes every run; the shape of the body (`{"uptime_ms":<int>,"status":"ok"}`) is what matters.
- `MG_ESC("ok")` renders as the literal `"ok"` in the body (including the quotes). Using plain `%s` here would emit unquoted garbage; the `%m` specifier is required.
- The server-side log line `GET /api/stats` comes from the `MG_INFO((...))` call inside the matched branch.

---

### Test 2 - Unknown URI Returns 404 JSON

**Purpose:** Verify that URIs the server does not know about fall through to the 404 branch and return a well-formed JSON error, not an empty body or an HTML default page.

**Code references:**
- [`http.c` L28-L34](../application/sources/app/http/http_server/http.c#L28-L34) - `else` branch of the URI check: `mg_http_reply` with status 404 and `{"error":"not found"}`.

**Setup:** two terminals.

**Terminal A - run the server:**
```bash
./http
```

**Terminal B - hit an unknown URI:**
```bash
curl -i http://127.0.0.1:8000/nonexistent
```

<table align="center">
  <tr>
    <td align="center"><em>Screenshot coming soon</em></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 2:</em></strong> Unknown URI returns 404 JSON</p>

**Reading the evidence:**

Terminal B (`curl -i`) output:
```
HTTP/1.1 404 Not Found
Content-Type: application/json
Content-Length: 22

{"error":"not found"}
```

**Observations:**
- Status is exactly `404 Not Found`; Mongoose derives the reason phrase from the numeric code passed to `mg_http_reply`.
- The body stays JSON, which is important for a real API: a browser default HTML 404 page would break clients that assume `Content-Type: application/json`.
- The current source does not log a line for the 404 case (the `MG_INFO((...))` call only fires inside the matched branch). Client-side output is the primary evidence for this test.

---

### Test 3 - Graceful Shutdown on SIGINT/SIGTERM

**Purpose:** Verify that pressing `Ctrl+C` (SIGINT) or receiving SIGTERM makes the main loop exit cleanly through `mg_mgr_free`, not through a kernel-level termination that would leak the listening socket.

**Code references:**
- [`http.c` L6-L11](../application/sources/app/http/http_server/http.c#L6-L11) - `s_quit` flag and the `on_sigint` handler that sets it.
- [`http.c` L44-L45](../application/sources/app/http/http_server/http.c#L44-L45) - handler registration for `SIGINT` and `SIGTERM`.
- [`http.c` L55-L61](../application/sources/app/http/http_server/http.c#L55-L61) - main loop checks `!s_quit`; after exiting, `Shutting down` is logged and `mg_mgr_free` is called.

**Setup:** one terminal.

**Terminal A - run the server and press Ctrl+C after a moment:**
```bash
./http
# (wait, optionally hit /api/stats from another terminal)
# Press Ctrl+C
```

<table align="center">
  <tr>
    <td align="center"><em>Screenshot coming soon</em></td>
  </tr>
</table>
<p align="center"><strong><em>Figure 3:</em></strong> Graceful shutdown on SIGINT</p>

**Reading the evidence:**

Terminal A (server log) after Ctrl+C:
```
Listening on http://0.0.0.0:8000
GET /api/stats
Shutting down
```

**Observations:**
- The `Shutting down` line proves that `s_quit` was set, the main loop exited on the current polling iteration, and control reached the post-loop code that calls `mg_mgr_free`.
- Without the signal handler the process would be killed by the default SIGINT action, no cleanup would run, and the listening socket would linger in `TIME_WAIT` until the kernel reclaimed it. Registering `SIGTERM` in addition to `SIGINT` covers `systemctl stop` and `kill <pid>` cases the same way.
- Same pattern as MQTT (see [`mqtt-reporting.md` Test 3](mqtt-reporting.md#test-3---graceful-disconnect-will-does-not-fire)): Mongoose does not trap signals; libc's `signal()` sets a flag, the main loop checks it, and cleanup runs after the loop.

---

## IX. Conclusion

### `mg_http_reply` Prints a Complete HTTP Response

`mg_http_reply(c, status, headers, fmt, ...)` writes the status line, `Content-Type` header (from the `headers` argument), `Content-Length` (computed from the formatted body), and the body itself. The application only supplies the status code, any custom headers, and the printf-style body.

### `%m` + `MG_ESC` Is How You Emit JSON String Values

The `%m` specifier defers formatting to a helper function whose printer is provided as the matching argument, usually `MG_ESC("...")` for JSON-quoted output. Using plain `%s` for a JSON string value produces raw bytes and breaks the format. Use `%lu`, `%d`, etc. for numeric values.

### Signal Handling Is a libc Concern, Same as MQTT

Mongoose does not intercept signals. The application must register a `signal()` handler that sets a flag, and the main loop must check that flag. This pattern is identical to the MQTT client (see [`mqtt-reporting.md` Conclusion](mqtt-reporting.md#signal-handling-belongs-to-libc-not-mongoose)) and should be considered the standard shape for every Mongoose-based application in this repository.

### Route Matching Is Explicit

There is no automatic dispatch table. Every route is an explicit `mg_match` check inside `ev_handler`, and the `else` branch is the effective 404. Adding more routes means adding more `mg_match` clauses, in whatever order the application prefers (first match wins because of the `if / else if / else` chain).

---

## X. Future Work

The current source is intentionally minimal so it can be studied end-to-end in one file. The following features are planned as follow-up study cases and will be added once implemented in the source:

- **Static file serving** with `mg_http_serve_dir(c, hm, &opts)` for a document root, then a `curl` test that fetches an index page.
- **HTTPS** with `mg_tls_init()` inside `MG_EV_ACCEPT`, mirroring the MQTT TLS setup in [`mqtt-reporting.md` Test 7](mqtt-reporting.md#test-7---tls-connecting-via-mqtts-with-a-self-signed-ca) (self-signed CA, mbedTLS backend).
- **POST body handling** with `mg_http_read_body` to accept and echo a JSON payload, exercising the request-body side of `mg_http_message`.
- **Port to hardware:** wire the same `http.c` against an embedded TCP/IP stack on a real board.

---

## XI. References

- Mongoose upstream repository: <https://github.com/cesanta/mongoose>
- Cesanta documentation hub: <https://mongoose.ws/documentation/>
- HTTP Server tutorial: <https://mongoose.ws/documentation/tutorials/http/http-server/>
- REST server tutorial: <https://mongoose.ws/documentation/tutorials/http/rest-server/>
