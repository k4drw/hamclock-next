# New Features in HamClock-Next

This page documents features that are **new or substantially changed** in HamClock-Next relative to the original HamClock by WB0OEW. It is written for users who know the original and want to understand what has been added.

Features are grouped by category. Each entry includes a brief description and, where relevant, the configuration field or UI location.

---

## Platform & Rendering

### SDL2-Based Rendering (Multi-Platform)

The original HamClock used a custom framebuffer/X11 rendering stack that limited it to Linux. HamClock-Next is built on **SDL2**, which enables:

- **Linux** — framebuffer and native window, including Raspberry Pi (ARM)
- **macOS** — native window (Intel and Apple Silicon)
- **Windows** — native x64 binary and NSIS installer (`HamClock-Next-Setup.exe`)
- **Browser** — via WebAssembly (Emscripten), runs in any modern browser with no installation

### Browser / WebAssembly Build

Run HamClock-Next entirely in a browser tab. Build with `./scripts/build-wasm.sh` (or the Docker variant). The browser build is functionally identical to the native build — all widgets, overlays, and configuration work the same way.

A CORS proxy setting (`corsProxyUrl`) routes network requests through a same-origin proxy to satisfy browser security restrictions.

### High-DPI / Letterbox Mode

On wide or 4K displays, HamClock-Next renders at the native logical resolution and letterboxes within the physical window, keeping all UI elements correctly proportioned regardless of window size or display scale.

### CI/CD Build Targets

Automated builds are produced for: **x86-64**, **ARM64**, **ARMhf**, and **WASM**. An OpenSuSE Tumbleweed **RPM package** is also produced.

---

## Layout

### Pane Rotation — Manual Cycling and Configurable Interval

The original HamClock already supported multiple widgets per pane with auto-rotation, enforcing one instance of each widget across all panes. HamClock-Next keeps that model and adds:

- **Manual cycling** — left/right arrow buttons on each pane let you step forward or back through the pane's widget list immediately, without waiting for the auto-rotate timer; advancing manually resets the timer
- **Configurable rotation interval** — the auto-rotation dwell time is user-settable (default 30 seconds); in the original this was fixed
- **Sync rotation** — all panes can be configured to advance simultaneously rather than independently (`syncRotation`), creating a coordinated "page turn" effect across the whole dashboard

### Widget/Map Groups (Presets)

The original HamClock had no way to save and recall a complete dashboard configuration. HamClock-Next adds a **Presets system** — named groups that capture the widget rotation lists for all panes together with the active map overlays and propagation settings. A single click applies the entire group at once.

See the [Presets](#presets-system) section below for details.

### Side Panel Mode

The right column (normally Panes 4 and 5) can be switched to a **full-height side panel** mode. Options:

| Mode                                        | Description                                         |
| ------------------------------------------- | --------------------------------------------------- |
| DE Info + DX/Sat (two panes, original-like) | Standard two-pane layout                            |
| DX Cluster (full height)                    | DX cluster spot list fills the full right column    |
| On The Air (full height)                    | POTA/SOTA activations fill the full right column    |
| Live Spots (full height)                    | PSK Reporter / RBN spots fill the full right column |

Side panel mode is selectable from Setup → Widgets, or by clicking the title bar of the side pane.

---

## New Widgets

The following widgets have **no equivalent** in the original HamClock:

| Widget            | Description                                                                                                                                                                                    |
| ----------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Asteroid**      | Lists the next 5 close Earth approaches from the JPL Small Solar System Bodies Database                                                                                                        |
| **Ionosonde**     | Live ionospheric sounding data from a remote ionosonde network                                                                                                                                 |
| **Lightning**     | Real-time lightning strike activity map (Blitzortung)                                                                                                                                          |
| **Meteor**        | Meteor scatter activity levels and upcoming shower calendar                                                                                                                                    |
| **Solar Storm**   | Current NOAA geomagnetic storm watch / warning / alert status                                                                                                                                  |
| **Tropo**         | Tropospheric ducting forecast index                                                                                                                                                            |
| **Sys Info**      | System resource display — CPU, memory, network, uptime                                                                                                                                         |
| **Santa Tracker** | Tracks Santa's position on Christmas Eve — in the original this was a hidden easter egg that auto-activated on the map with no user control; in HamClock-Next it is a proper selectable widget |

---

## Propagation Overlays

HamClock-Next adds several propagation overlays not present in the original:

| Overlay                 | Description                                                                                                                     |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------- |
| **MUF Real-Time**       | Maximum Usable Frequency derived from live NOAA ionospheric data (KC2G model); shows where your selected band is open right now |
| **VOACAP Area**         | VOACAP-based propagation reliability from DE to all world areas; configurable band, mode, and power                             |
| **VOACAP Point**        | VOACAP prediction from DE to a specific DX target point                                                                         |
| **VOACAP Reliability**  | Circuit reliability percentage map                                                                                              |
| **VOACAP TOA**          | Take-Off Angle prediction using geometric F2-layer model (multi-hop capable)                                                    |
| **Propagation Heatmap** | Live PSK Reporter spot density heat map — shows where your signal is actually being heard right now                             |
| **DRAP**                | D-Region Absorption Prediction map (also available as a standalone widget)                                                      |

All overlays are configurable by **band**, **mode**, and **power** (watts).

---

## Weather Overlays

| Overlay                | Description                                                                           |
| ---------------------- | ------------------------------------------------------------------------------------- |
| **Cloud Cover**        | Global cloud imagery from NASA GIBS satellite composite                               |
| **WX Pressure (WxMb)** | Surface pressure contours from NOAA NOMADS GRIB2 data, rendered with Marching Squares |

---

## Map Enhancements

- **Robinson projection** — an additional map projection option alongside Mercator (equirectangular) and Azimuthal
- **POTA activator map pins** — lime-green pins on the map for active POTA activations (requires On The Air widget in a pane)
- **ADIF QSO map pins** — plots your logged QSOs from an ADIF file on the map (requires ADIF widget)
- **Satellite ground track** — the selected satellite's orbital path is drawn as an arc on the map (`showSatTrack`)
- **Beacon/widget-aware plotting** — beacon markers and Live Spots map pins are only drawn when the corresponding widget is actually in a pane's rotation list

---

## DX Cluster & Live Spots Enhancements

- **DX panel spot selection** — clicking a spot row in the DX Cluster or On The Air panel drives the DX Info panel (callsign, grid, DXCC entity details); mutual exclusion between panels
- **Inline spot label on map** — when a DX cluster spot is selected, the callsign, frequency, and band are shown inline next to the map bubble (no hover required)
- **Live Spots in-widget band selection** — the original supported RBN, PSK Reporter, and WSPR as sources, and had band filtering, but both were configured through a separate widget config menu. In HamClock-Next, bands are toggled by clicking them directly in the widget; source selection is also surfaced inline rather than buried in a config dialog

---

## Presets System

A **★ (star) button** in the Time Panel opens the Presets modal. Save named configuration snapshots and recall them instantly. Each preset captures:

- All six pane widget rotation lists
- Rotation interval
- Active propagation and weather overlays
- Map style, night lights, grid settings
- Propagation band, mode, and power

Presets make it simple to switch between operating contexts (contest, casual DX, morning check, etc.) without manual reconfiguration.

---

## Hardware Integrations

| Integration           | Description                                                                                                |
| --------------------- | ---------------------------------------------------------------------------------------------------------- |
| **Hamlib rotctld**    | Connect to an antenna rotator via `rotctld`; optional auto-track to DX bearing                             |
| **Hamlib rigctld**    | Connect to a transceiver via `rigctld`; optional auto-tune to propagation band frequency                   |
| **GPS**               | Use a GPS receiver for automatic location (lat/lon)                                                        |
| **BME280 I2C sensor** | Local temperature/humidity/pressure from a connected BME280 sensor (partial — hardware validation pending) |

---

## Audio

- **Countdown chime** — the Countdown widget plays a procedural chime when the timer reaches zero (`SoundManager`)

---

## Configuration Improvements

- **JSON configuration file** — human-readable, portable, and version-controllable (replaces original binary/EEPROM format)
- **Automatic migration** — the config loader auto-migrates legacy flat keys from earlier HamClock-Next versions
- **Brightness schedule** — configure dim and bright times (hour:minute) for automatic display brightness control
- **Color overrides** — per-element color customization via `colorOverrides` map
- **Hub mode** — run HamClock-Next as a local data server (`Server`) that other instances act as clients of (`Client`), enabling a shared data feed on a local network
- **RSS ticker** — optional scrolling RSS feed display
- **CORS proxy** — configurable proxy URL for browser (WASM) builds where direct cross-origin requests are blocked

---

## UI & UX Improvements

- **K key highlight mode** — press K to draw cyan bounding boxes around every interactive region on screen with tooltip labels; press K again to dismiss
- **Large Kp number overlay** — current Planetary K-index rendered as a large color-coded number over the bar chart in the Aurora Graph widget (storm level colors)
- **Contest detail popup** — clicking a contest row in the Contests widget opens a detail panel with exchange, rules link, and category summary
- **On The Air filter chip** — the original On The Air widget showed all POTA and SOTA activations with no way to separate them; HamClock-Next adds a filter chip (All / POTA / SOTA) so you can focus on one program at a time (`ontaFilter` config field)
- **Scrollable widget list in Setup** — the 44-widget checklist in Setup → Widgets scrolls with mouse wheel and `^`/`v` arrows when it overflows
- **Worker thread pool** — background tasks (propagation engine computation, RSS parsing) run in a thread pool so the UI never blocks
- **QRZ premium callbook** — in addition to free Callook and HamDB lookups, QRZ.com XML API is supported for subscribers

---

## What Is Not Yet Implemented

A small number of features are partially implemented or require additional configuration:

| Feature                    | Status                                                           |
| -------------------------- | ---------------------------------------------------------------- |
| Repeater directory         | Implemented; disabled until a RepeaterBook API key is configured |
| Winlink gateway listing    | Implemented; disabled until Winlink API access is configured     |
| Interactive web config UI  | Deferred to a future release                                     |
| BME280 hardware validation | Code complete; needs physical RPi + sensor to confirm            |
| K key tooltip coverage     | Not all widgets return full action lists; ongoing improvement    |
