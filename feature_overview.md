# HamClock-Next — Feature Overview

> **HamClock-Next** is a modern, multi-platform rewrite of the original HamClock
> by **Elwood Downey, WB0OEW** (SK 29 January 2026).
>
> Elwood built HamClock into one of the most beloved shack displays in amateur
> radio — a real-time world clock, propagation tool, and DX dashboard all in one.
> He maintained it for years with care and craftsmanship, and the ham community
> is lesser for his passing. HamClock-Next carries his work forward with deep
> respect, preserving every feature of the original while adding a modern
> architecture and new capabilities for today's ham shacks.
>
> *This project is dedicated to his memory. 73, Elwood.*

---

## Why a Complete Rewrite?

HamClock-Next did not start as a rewrite. It started as a refactor — and a
frustrating one.

Elwood originally wrote HamClock for the **ESP8266** microcontroller. Over time
he shimmed in Linux support around that embedded core. By version 4 he had
formally dropped ESP8266 support, but the architectural fingerprint of
single-file globals, Arduino-style execution flow, and tight coupling to his own
propagation proxy remained woven throughout the codebase. It was never the kind
of code you could fault — it was *brilliant* embedded engineering — but it was
also not a foundation you could cleanly modernize.

I spent a week attempting it anyway: reorganizing translation units, untangling
the proxy dependency, trying to introduce proper RAII and thread-safety. Every
session was a version of the same story — fix one thing, break two others.
The root cause wasn't any single piece of code; it was that the whole structure
was load-bearing in ways that only revealed themselves mid-refactor.

The decision to start clean was not made lightly, but it was the right call.
A greenfield CMake + SDL2 + modern C++17 project with a proper layer separation
(data stores, providers, UI widgets, network) turned out to move faster than
the refactor had — and the result is something that can actually be maintained,
extended, and ported without archaeology.

---

## Project Philosophy — AI as Co-Developer

HamClock-Next is, by deliberate design, a near-**100% AI-written codebase**.
I serve as the architect: I define the structure, own the design decisions,
set the acceptance criteria, and review every output. The implementation itself
is written by AI.

This was a conscious experiment, driven by two things happening at the same time:

- My EVP has been pushing our engineering organization to deeply integrate AI
  into day-to-day work — not as a novelty, but as a genuine force multiplier.
  I wanted firsthand data on what that actually looks like at the scale of a
  real, shippable application.
- I was genuinely curious. What does a production-quality, AI-written C++
  codebase look like when the human focuses entirely on architecture?

The tools used throughout development:

| Tool                               | Role                                                    |
| ---------------------------------- | ------------------------------------------------------- |
| **Antigravity** (Gemini 3.0 Flash) | Primary implementer — found to be the strongest for C++ |
| **Gemini CLI**                     | Quick lookups, one-off generation                       |
| **Claude Code**                    | Secondary review and alternative implementations        |

The answer to the experiment so far: it works. v1.0B01 has shipped with all 71
original features and over 10 new ones. The code compiles clean, passes
valgrind, and runs stably on everything from a Raspberry Pi 3 to a browser tab.

---

## The MCP Ecosystem

Two Model Context Protocol servers were built alongside this project — one for
the project itself, one for work.

### hamclock-next-mcp

The `hamclock-next-mcp` was built as a hands-on test of the MCP pattern.
It gives the AI development session direct, structured access to the project:
feature parity tracking, code scaffolding for new widgets, symbol search across
both the original HamClock source and HamClock-Next, memory diagnostic queries,
and VOACAP overlay tooling — all available as tool calls without leaving the
coding context. It paid for itself immediately in reduced context-switching and
more accurate code generation.

### cloud-readiness-mcp *(work project)*

The pattern proved itself here, so I applied it to a real organizational problem
at work. Product and Project Managers had begun vibe-coding web applications and
arriving at DevOps asking us to "just host" them — with no containers, no
resource limits, no consideration for EKS deployment, and no documentation.

The `cloud-readiness-mcp` changes that flow. It guides developers (and
non-developers) through proper code structure, containerization best practices,
and EKS deployment readiness using scaffolding built right into the tool. Before
a team even has a conversation with DevOps, they run the MCP, which produces a
structured readiness report that the team can review. The report becomes the
agenda for the handoff meeting — or eliminates the need for one entirely.

The primary beneficiaries: myself as Principal DevOps Engineer, the rest of the
DevOps team, and the development teams who will eventually own and maintain the
code they ship.

---

## 100% Feature Parity with the Original HamClock

HamClock-Next implements all **71 original features** across every category:

| Category                | Features                                                                                                                                 |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| World Map & Projections | Azimuthal, Robinson, Mercator; day/night terminator; great-circle paths; Maidenhead/lat-lon grid overlays                                |
| Map Markers             | City labels, DX Cluster spots, PSK Reporter spots, POTA/SOTA activator pins, ADIF QSO pins, satellite tracks                             |
| Map Overlays            | VOACAP MUF, VOACAP Reliability, VOACAP TOA, KC2G real-time MUF, cloud cover, WX precipitation                                            |
| Data Panels             | Solar/space weather, DX Cluster, Live Spots (PSK/WSPR/RBN), POTA/SOTA activity, satellite tracking, contest calendar, RSS feed, and more |
| Core UI                 | Clock display, callsign, DE/DX info panels, bearing/distance, countdown timer, audio alarms                                              |
| Hardware                | GPS/NMEA (gpsd), rotator control (rotctld), rig CAT control (rigctld), display brightness, display power management                      |
| Utilities               | QRZ.com lookup, Maidenhead grid system, callsign prefix lookup, EME (moonbounce) tool, Santa Tracker                                     |

---

## 🆕 New Features Exclusive to HamClock-Next

### 1. Multi-Display Hub Model

The flagship new architecture. One HamClock-Next instance runs as a **Master hub**
and fetches all external data (space weather, DX spots, propagation, etc.). All other
instances on the same LAN run as **Clients** and pull data from the hub instead of
hitting the internet directly.

- Eliminates redundant API calls across multiple displays in a club station or multi-monitor shack
- Unified, consistent data across every screen — no clock-skew or stale-data mismatches
- Hub handles all rate-limiting and caching; clients are lightweight
- Works transparently — client instances configure a single hub hostname; the rest is automatic

### 2. Smart Network Caching

Every external data fetch is cached both in memory and on disk.

- **ETag / HTTP 304 support** — avoids re-downloading data that hasn't changed
- **Per-type TTL** — space weather refreshes every 5 minutes; aurora history every hour; asteroid data every 24 hours
- Dramatically reduces bandwidth on slow or metered connections (critical for Raspberry Pi cellular setups)
- Cache survives restarts — data is available immediately on next launch

### 3. Live Web Viewer & Remote Control

- MJPEG live stream of the HamClock screen, viewable in any browser on your LAN
- Full **mouse and keyboard passthrough** — click and type through the browser as if you were at the display
- `--web-only` / headless mode — run on a server with no physical display; access everything via browser
- REST API with 30+ endpoints for automation, scripting, and integration with home-automation systems

### 4. Advanced Weather & Environment

- **7-day NWS forecast panel** — hourly and daily outlook for your grid square
- **NOAA severe weather alerts** — active watches, warnings, and advisories for your county
- **Hurricane / tropical cyclone tracker** — active storm list with track and intensity
- **Marine / tide panel** — NOAA tide predictions for nearby stations
- **BME280 I2C sensor** — reads local temperature, humidity, and barometric pressure directly from hardware attached to a Raspberry Pi

### 5. Ham Radio Hardware Integration

- **Hamlib rig control** — reads frequency and mode from your transceiver via `rigctld`; drives the DX panel automatically
- **Hamlib rotator control** — displays current azimuth and elevation; accepts bearing commands from the map
- **WSJT-X / JS8Call UDP** — dedicated port listener captures QSO data live from digital mode software
- Clicking a DX spot on the map auto-populates the DX panel and can command the rotator

### 6. Expanded Radio Activity & Logging

- **ADIF log import** — plots confirmed QSOs as pins on the world map; visualize your log geographically
- **DX watchlist** — monitor specific callsigns or prefixes; on-screen alert when a watched station is spotted
- **ONTA (On The Air) panel** — aggregated POTA/SOTA activations with band-color-coded spot display

### 7. Space & Celestial

- **Asteroid Tracker** — next 5 close approaches from JPL SSD/CAD data, including miss distance, relative velocity, and Potentially Hazardous Asteroid flag; cached 24 hours
- **History Panel** — scrollable time-series graphs of solar flux (SFI), sunspot number (SSN), planetary K-index, and geomagnetic Ap index
- **Large Kp overlay** — current Planetary K rendered as a large color-coded number over the Kp bar chart (green / yellow / red by storm level)
- **Native VOACAP propagation engine** — MUF, reliability, and takeoff-angle heatmaps computed locally, no external service required

### 8. Platform & Build

- **Linux** — x86_64 and ARM (Raspberry Pi 3/4/5, armhf and arm64); framebuffer and X11/Wayland
- **macOS** — Intel and Apple Silicon; native `.app` bundle
- **Windows x64** — cross-compiled executable and NSIS installer (`HamClock-Next-Setup.exe`)
- **WebAssembly / Browser** — full Emscripten build; serve from any web server; no install required
- **OTA update notifications** — version string turns amber when a newer release is available on GitHub

### 9. Security & Reliability

- **Private-network gate** — embedded web server refuses to bind on public IP interfaces by default; no accidental internet exposure
- **SSRF guard** — hub proxy requests block access to RFC-1918, loopback, IPv6 ULA, and link-local addresses
- **Input validation** — all web API parameters length-capped and sanitized server-side
- **DX Cluster spot cap** — hard limit of 500 spots prevents unbounded memory growth on busy telnet clusters
- **Worker thread pool** — VOACAP heatmap computation and RSS parsing run in background threads; UI stays responsive

### 10. Modern UI Enhancements

- **Dynamic Theme Engine** — Multiple built-in color themes (Default, Dark, Glass) plus a powerful **Custom Theme** editor.
- **Real-time UI Preview** — Changes in the Theme Customizer are reflected immediately across the entire UI before saving.
- **UI Standardization** — Unified interaction patterns (PascalCase "Done/Cancel" buttons), consistent sizing, and semantic theme-aware coloring for all interactive elements.
- **Adaptive layouts** — panels reflow for 7-inch (1024×600) displays and larger 1080p/4K screens.
- **Manual pane rotation** — left/right arrows in multi-pane view; resets auto-rotate timer
- **DX spot inline label** — callsign, frequency, and band shown next to selected map bubble without requiring hover
- **Semantic click API** — external tools can trigger named widget actions (`/debug/click?widget=SolarPanel&action=Cycle`) without pixel-coordinate guessing

---

## REST API & Automation

The embedded web server (default port 8080) exposes **30+ endpoints**:

- `GET /` — live MJPEG browser viewer with mouse/keyboard passthrough
- `GET /live.jpg` — current screen as a JPEG snapshot
- `GET /get_config.txt`, `/get_time.txt`, `/get_spacewx.txt`, `/get_de.txt`, `/get_dx.txt` — plain-text telemetry (original HamClock format; compatible with existing scripts)
- `POST /api/display/power` — turn display on/off (also supports Raspberry Pi `vcgencmd` and `bl_power` backlight methods)
- `GET /set_newde`, `/set_newdx`, `/set_mappos` — set DE or DX location by grid square, callsign, or lat/lon
- `GET /set_cluster`, `/set_title` — update cluster host and station callsign remotely
- `GET /debug/widgets`, `/debug/health`, `/debug/performance`, `/debug/logs` — diagnostics and automation hooks
- Full legacy compatibility with original HamClock URL endpoints — existing hardware controllers and scripts work without modification

See [API.md](API.md) for the complete endpoint reference.

---

## Platform Support

| Platform                                       | Status                        |
| ---------------------------------------------- | ----------------------------- |
| Linux x86_64 (framebuffer / X11)               | ✅ Supported                   |
| Linux ARM64 (Raspberry Pi 4/5, Armbian 64-bit) | ✅ Supported (arm64)           |
| Linux ARMhf (Raspberry Pi 3, 32-bit OS)        | ✅ Supported (armhf)           |
| macOS Apple Silicon                            | ✅ Supported                   |
| Windows x64                                    | ✅ Supported (exe + installer) |
| Browser (WebAssembly)                          | ✅ Supported                   |

---

## Get HamClock-Next

Releases, installers, and WASM builds are available at:

**https://github.com/k4drw/hamclock-next/releases**

HamClock-Next is free and open source under the GPL license,
in the spirit of the original work by Elwood Downey, WB0OEW.

*73 de K4DRW*
