<div align="center">

![Repo Traffic](https://komarev.com/ghpvc/?username=embedded-application-stack&label=Repo+Traffic&color=blue&style=flat-square)

</div>

<p align="center">
  <img src="https://img.shields.io/badge/language-C-red?style=flat-square" alt="Language">
  <img src="https://img.shields.io/badge/library-Mongoose-red?style=flat-square" alt="Library">
  <img src="https://img.shields.io/badge/tls-mbedTLS-red?style=flat-square" alt="TLS">
  <img src="https://img.shields.io/badge/platform-Linux%20(study)-red?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/license-MIT-red?style=flat-square" alt="License">
</p>

# Embedded Application Stack

<!-- Banner: drop the file into resources/images/thumbnail/ and update the src below -->
<center><img width="1280" alt="Embedded Application Stack" src="../resources/images/thumbnail/banner_embedded_application_stack.png" />
</center>

A connectivity stack for embedded applications - MQTT, HTTP/HTTPS, TLS - built on top of the [Mongoose](https://github.com/cesanta/mongoose) networking library. The end goal is to run this on a real board; the project is currently in the study phase, exercising each protocol on Linux before porting to hardware.

## Documentation

| File | Description |
| --- | --- |
| [README.md](README.md) | Project overview, module status, repository structure, and contact information. |
| [docs/mqtt-reporting.md](docs/mqtt-reporting.md) | MQTT study report on Linux with local Mosquitto: sub/pub/unsub, Last Will, graceful disconnect, auto-reconnect, user/password authentication, and TLS with mbedTLS. |
| [docs/http-reporting.md](docs/http-reporting.md) | HTTP server study report on Linux: listener setup, `/api/stats` JSON route, 404 fallback, and graceful shutdown; plus a Future Work section for static files, HTTPS, and POST body handling. |

## Introduction

This repository is a personal study project that walks through the connectivity protocols commonly used on embedded devices. Each module is implemented as a small, self-contained program that links directly against the Mongoose amalgamation, so you can read one folder end-to-end without navigating a build system.

The immediate objective is to reach a firm understanding of every protocol on a Linux host, then port the same code to a real MCU with only the transport layer changing. The `mongoose.c` and `mongoose.h` files are duplicated per module on purpose - it keeps each folder buildable in isolation and makes it easy to swap in a different upstream version for a single module without touching the others.

## Module Status

<div align="center">

| Module | Status | Report |
| :---: | :---: | :---: |
| MQTT | Study complete on Linux (local Mosquitto): sub/pub/unsub, Last Will, graceful disconnect, auto-reconnect, user/password auth, TLS with mbedTLS | [docs/mqtt-reporting.md](docs/mqtt-reporting.md) |
| HTTP | Minimal working server on Linux: listener, `/api/stats` JSON route, 404 fallback, graceful shutdown. Static files and HTTPS listed as future work | [docs/http-reporting.md](docs/http-reporting.md) |

</div>

> **Note:** No module has been ported to real hardware yet. All evidence in the reports comes from a Linux host.

## Repository Structure

```
embedded-application-stack/
├── application/
│   └── sources/
│       └── app/
│           └── <module>/               # one folder per protocol (mqtt, http, ...)
│               └── <module>_<role>/    # per-role subfolder (mqtt_client, http_server, ...)
│                   ├── lib/            # Mongoose amalgamation (mongoose.c, mongoose.h)
│                   ├── <name>.c        # module source (mqtt.c, http.c)
│                   ├── Makefile        # gcc build, links mongoose.c (and mbedTLS for MQTT)
│                   └── .gitignore      # local build artefacts
├── docs/
│   └── <module>-reporting.md           # study report for the module
└── resources/
    └── images/
        ├── <module>/                   # screenshots used as evidence in reports
        └── thumbnail/                  # banner and other README artwork
```

For per-module details, follow the report link in the [Documentation](#documentation) table.

## Contact & Support

<p style="font-size: 20px;"><strong>Cao Trong Phuoc</strong> - Software Engineer - Embedded Systems</p>

``` Note
Thank you for visiting this repository.
If you have any questions, suggestions, or feedback about this project, feel free to reach out directly.
```

<a href="https://github.com/caotrongphuoc">
  <img src="https://img.shields.io/badge/GitHub-caotrongphuoc-181717?style=for-the-badge&logo=github&logoColor=white"/>
</a>

<a href="https://www.linkedin.com/in/cao-trong-phuoc/">
  <img src="https://img.shields.io/badge/LinkedIn-Cao%20Trong%20Phuoc-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white"/>
</a>
