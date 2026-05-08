# New Features in HamClock-Next

This page covers the features that are new or meaningfully changed in HamClock-Next compared with the original HamClock by WB0OEW. It is written for users who already know the original and want to see what is different.

Features are grouped by category. Each entry includes a brief description and, where relevant, the configuration field or UI location.

---

## Platform & Rendering

### Multi-Platform Support

The original HamClock was limited to Linux. HamClock-Next runs on:

- **Linux** — framebuffer and native window, including Raspberry Pi (ARM)
- **macOS** — native window (Apple Silicon)
- **Windows** — native x64 binary and NSIS installer (`HamClock-Next-Setup.exe`)
- **Browser** — runs in any modern browser with no installation

### Browser Build

Run HamClock-Next entirely in a browser tab with no installation. The browser version works like the desktop build, so the same widgets, overlays, and settings are available.

If you are hosting it on your own server, see `corsProxyUrl` in [Setup & Configuration](Configuration.md) and the [CORS proxy](Glossary.md#cors-proxy).

### High-DPI / Letterbox Mode

On wide or 4K displays, HamClock-Next renders at the native logical resolution and letterboxes within the physical window, keeping all UI elements correctly proportioned regardless of window size or display scale.

### Available Packages

Pre-built packages are available for: **64-bit PC** (Linux and Windows), **Raspberry Pi** (64-bit and 32-bit ARM), and **Browser**. An OpenSUSE Tumbleweed RPM package is also available.

### RPi3B Stability

Memory use has been substantially reduced for low-resource devices like the Raspberry Pi 3B. HamClock-Next now runs reliably on boards that previously ran out of memory and crashed.

### Debug API Build Flag

Use the `--enable-debug-api` flag with build scripts (e.g., `./scripts/build.sh`) to easily enable local testing of the REST Debug API.

---

## Stability & Network Improvements

### Remote Control Stability

Remote control commands sent via the [REST API](REST-API.md) are now synchronized with the display. This avoids visual glitches when commands arrive faster than the screen can update and helps logging software or automation scripts.

### Network Data Sharing (LAN Hub)

If you run multiple HamClock-Next instances on the same local network, one can act as the **Master** and share its downloaded data with the others. This reduces internet traffic and prevents all instances from hammering the same data sources simultaneously. Client instances fall back to fetching directly if the Master is unavailable.

### Memory Churn & High-Throughput Overhaul

High-throughput background services (such as SOTA/POTA parsing, NOAA Aurora grid analysis, and LoTW Activity Store queries) have been completely overhauled to use zero-allocation `std::string_view` split algorithms, static thread-local rendering buffers, and moved data structures to eliminate frame-to-frame heap churn and reduce parsing latency by over 50%.

### Robust Local Caching

The caching system has been enhanced with a strict 5,000 metadata entry limit, LRU eviction for metadata-only entries, binary-safe cache writes, RAM caching bypass for large files (>512KB) to prevent Master Hub bloat, and epoch-independent timestamp checking to prevent unnecessary local cache bypass on application restart.

---

## Layout

### Pane Rotation — Manual Cycling and Configurable Interval

The original HamClock already supported multiple widgets per pane with auto-rotation. HamClock-Next keeps that model and adds:

- **Manual cycling** — left/right arrow buttons on each pane let you move through the widget list immediately
- **Configurable rotation interval** — the auto-rotation delay is user-settable (default 30 seconds)
- **Sync rotation** — all panes can advance together instead of independently

### Widget/Map Groups (Presets)

The original HamClock had no way to save and recall a complete dashboard configuration. HamClock-Next adds a **Presets system** that saves the widget rotation lists for all panes together with the active map overlays and propagation settings.

See the [Presets](#presets-system) section below for details.

### Side Panel Mode

The **left column** (normally Panes 5 and 6) can be switched to a **full-height side panel** mode. Options:

| Mode                                        | Description                                         |
| ------------------------------------------- | --------------------------------------------------- |
| DE Info + DX/Sat (two panes, original-like) | Standard two-pane layout                            |
| DX Cluster (full height)                    | DX cluster spot list fills the full left column     |
| On The Air (full height)                    | [POTA / SOTA](Glossary.md#pota--sota) activations fill the full left column     |
| Live Spots (full height)                    | [PSK Reporter / RBN](Glossary.md#psk-reporter--rbn--wspr) spots fill the full left column  |

Side panel mode is selectable from Setup → Widgets, or by clicking the title bar of the side column.

---

## New Widgets

The following widgets have no equivalent in the original HamClock:

| Widget            | Description                                                                                                                                                                                    |
| ------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Asteroid**      | Lists the next 5 close Earth approaches from the JPL Small Solar System Bodies Database                                                                                                        |
| **Ionosonde**     | Live ionospheric sounding data from a remote ionosonde network                                                                                                                                 |
| **Lightning**     | Real-time lightning strike activity map (Blitzortung)                                                                                                                                          |
| **Meteor**        | Meteor scatter activity levels and upcoming shower calendar                                                                                                                                    |
| **Solar Storm**   | Current NOAA geomagnetic storm watch / warning / alert status                                                                                                                                  |
| **Tropo**         | Tropospheric ducting forecast index                                                                                                                                                            |
| **Sys Info**      | System resource display: CPU, memory, network, uptime, and GPU cache statistics (texture, vertex, and command buffer usage) for debugging.                                                                                                         |
| **World Clock**   | Up to 4 configurable time zones with city labels and UTC offsets                                                                                                                               |
| **Big Clock**     | High-visibility digital or analog clock with user-selectable color themes                                                                                                                     |
| **Solar (Basic)** | A condensed version of the Solar widget for simpler space weather monitoring                                                                                                                   |
| **DE Info (Basic)**| Home-station details without extra propagation metrics                                                                                                                                         |
| **DX Info (Basic)**| Target-station details                                                                                                                                                                         |
| **Santa Tracker** | Tracks Santa's position on Christmas Eve; in HamClock-Next it is a selectable widget                                                                                                           |

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
| **VOACAP Timeline**     | Reoriented X-axis so the leftmost column is always the current UTC hour ("now at origin"), with a vertical white line cursor. |
| **Propagation Heatmap** | Live [PSK Reporter / RBN / WSPR](Glossary.md#psk-reporter--rbn--wspr) spot density heat map — shows where your signal is actually being heard right now                             |
| **DRAP**                | D-Region Absorption Prediction map (also available as a standalone widget)                                                      |

All overlays are configurable by **band**, **mode**, **power**, **Take-Off Angle (TOA)**, and **path (Short/Long)**.

### Custom Propagation Colormaps

User-defined colormaps let you tune map rendering for MUF, Reliability, and TOA overlays. Choose between **Muted**, **Vibrant**, or **Custom**.

---

## Weather Overlays

| Overlay                | Description                                                                                                                                                                                                                               |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Cloud Cover (GFS)**  | High-fidelity layered cloud system from NOAA GFS model (GRIB2, 0.25° / 1440x721). Blends Low, Middle, and High altitude data with transparency tuning. Features "Seam Stitching" to ensure gap-free viewing at the Date Line antimeridian. |
| **WX Pressure (WxMb)** | Surface pressure contours from NOAA NOMADS GRIB2 data, rendered with high-fidelity Marching Squares contours. |

---

## Map Enhancements

- **Robinson projection** — an additional map projection option alongside Mercator and Azimuthal
- **Single Azimuthal projection** — a single-circle view centered on your location
- **Interactive Zoom & Pan** — use the mouse wheel to zoom and drag to pan the map
- **POTA activator map pins** — lime-green pins for active [POTA](Glossary.md#pota--sota) activations
- **ADIF QSO map pins** — plots your logged QSOs from an ADIF file on the map
- **ADIF/LoTW Map Pin Hover Tooltips** — hovering over ADIF/LoTW map pins displays a dynamic tooltip showing callsign, date, band, and mode, filtered by active settings
- **LOTW QSO Map Toggle** — toggle the display of LoTW/ADIF QSO pins on/off via the "LOTW QSOs" checkbox in the Map View options menu
- **Satellite ground track** — the selected satellite's orbital path is drawn as an arc on the map
- **Beacon/widget-aware plotting** — beacon markers and Live Spots pins are only drawn when the related widget is active

---

## DX Cluster & Live Spots Enhancements

### Duplicate Spot Hiding

The DX Cluster panel can be configured to "Hide duplicates (one per call/band)". When enabled, only the most recent spot for a station on a given amateur band is displayed, significantly cleaning up the view during contests or pileups.

- **DX panel spot selection** — clicking a spot row in the DX Cluster or On The Air panel updates the DX Info panel
- **Inline spot label on map** — when a DX cluster spot is selected, the callsign, frequency, and band are shown next to the map bubble
- **Live Spots in-widget band selection** — bands are toggled by clicking them directly in the widget, and source selection is shown inline

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
| **Hamlib rotctld**    | Connect to an antenna rotator via [`rotctld`](Glossary.md#rotctld--rigctld); optional auto-track to DX bearing                             |
| **Hamlib rigctld**    | Connect to a transceiver via [`rigctld`](Glossary.md#rotctld--rigctld); optional auto-tune to propagation band frequency                   |
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
- **Service Credential Persistence** — sensitive credentials (LoTW, Clublog, etc.) are now persisted in `config.json` and manageable via the Web API.
- **Color overrides** — per-element color customization via `colorOverrides` map
- **RSS ticker** — optional scrolling RSS feed display
- **CORS proxy** — configurable relay URL for browser builds; see [CORS proxy](Glossary.md#cors-proxy)

---

## UI & UX Improvements

- **K key highlight mode** — press K to draw boxes around every interactive region on screen and show tooltips
- **? Help Panel** — press **?** to open a scrollable help panel with shortcuts, controls, and widget descriptions
- **Large Kp number overlay** — current Planetary K-index shown as a large color-coded number in the Aurora Graph widget
- **Contest detail popup** — clicking a contest row opens a detail panel with exchange, rules link, and category summary
- **On The Air / DX Cluster refinements** — band color legends are added in double-height mode, and Live Spots uses a simpler one-column layout
- **Marine Auto-Lookup** — the Marine widget includes a "Find Closest" button for the nearest NOAA tide and buoy stations
- **Global Time Zone Picker** — a centralized time zone modal in Setup sets a default time zone used by time-related widgets
- **Scrollable widget list in Setup** — the widget checklist in Setup → Widgets scrolls when it is too long to fit
- **Greyline DX scroll** — the Greyline DX widget list scrolls when more entities are active than fit in the pane
- **Background data fetching** — propagation calculations and news feed updates run in the background so the display stays smooth
- **QRZ premium callbook** — QRZ.com XML API is supported for subscribers, in addition to free Callook and HamDB lookups
- **Aux Clock time zone cycling** — click the Aux Clock widget to cycle through preset time zones. The selected time zone persists across restarts and can also be set via API: `/set_config?aux_tz_offset=-5&aux_tz_label=EST`

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
