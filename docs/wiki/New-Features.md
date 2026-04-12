# New Features in HamClock-Next

This page documents features that are **new or substantially changed** in HamClock-Next relative to the original HamClock by WB0OEW. It is written for users who know the original and want to understand what has been added.

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

Run HamClock-Next entirely in a browser tab with no installation. The browser version is functionally identical to the desktop build — all widgets, overlays, and configuration work the same way.

If you are hosting it on your own server, see `corsProxyUrl` in [Setup & Configuration](Configuration.md) and the [Glossary](Glossary.md#cors-proxy).

### High-DPI / Letterbox Mode

On wide or 4K displays, HamClock-Next renders at the native logical resolution and letterboxes within the physical window, keeping all UI elements correctly proportioned regardless of window size or display scale.

### Available Packages

Pre-built packages are available for: **64-bit PC** (Linux and Windows), **Raspberry Pi** (64-bit and 32-bit ARM), and **Browser**. An OpenSUSE Tumbleweed RPM package is also available.

### RPi3B Stability

Memory use has been substantially reduced for low-resource devices like the Raspberry Pi 3B. HamClock-Next now runs reliably on boards that previously ran out of memory and crashed.

---

## Stability & Network Improvements

### Remote Control Stability

Remote control commands sent via the [REST API](REST-API.md) are now synchronized with the display. This prevents visual glitches when commands arrive faster than the screen can update — useful for logging software or automation scripts that drive HamClock-Next programmatically.

### Network Data Sharing (LAN Hub)

If you run multiple HamClock-Next instances on the same local network, one can act as the **Master** and share its downloaded data with the others. This reduces internet traffic and prevents all instances from hammering the same data sources simultaneously. Client instances fall back to fetching directly if the Master is unavailable.

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

The **left column** (normally Panes 5 and 6) can be switched to a **full-height side panel** mode. Options:

| Mode                                        | Description                                         |
| ------------------------------------------- | --------------------------------------------------- |
| DE Info + DX/Sat (two panes, original-like) | Standard two-pane layout                            |
| DX Cluster (full height)                    | DX cluster spot list fills the full left column     |
| On The Air (full height)                    | POTA/SOTA activations fill the full left column     |
| Live Spots (full height)                    | PSK Reporter / RBN spots fill the full left column  |

Side panel mode is selectable from Setup → Widgets, or by clicking the title bar of the side column.

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
| **World Clock**   | Up to 4 configurable timezones with city labels and UTC offsets; includes an integrated configuration UI (gear icon).                                                                          |
| **Big Clock**     | High-visibility digital or analog clock with user-selectable color themes.                                                                                                                     |
| **Solar (Basic)** | A condensed version of the Solar widget for users who prefer simplified space weather monitoring.                                                                                              |
| **DE Info (Basic)**| Standalone home-station details (callsign, grid, lat/lon) without additional propagation metrics.                                                                                              |
| **DX Info (Basic)**| Standalone target-station details.                                                                                                                                                             |
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

All overlays are configurable by **band**, **mode**, **power**, **Take-Off Angle (TOA)**, and **path (Short/Long)**.

### Custom Propagation Colormaps

User-defined colormaps allow full control over map rendering for MUF, Reliability, and TOA overlays. Choose between **Muted** (safe for overlays), **Vibrant** (high visibility), or **Custom** (a 5-point linear gradient editor).

---

## Weather Overlays

| Overlay                | Description                                                                                                                                                                                                                               |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Cloud Cover (GFS)**  | High-fidelity layered cloud system from NOAA GFS model (GRIB2, 0.25° / 1440x721). Blends Low, Middle, and High altitude data with transparency tuning. Features "Seam Stitching" to ensure gap-free viewing at the Date Line antimeridian. |
| **WX Pressure (WxMb)** | Surface pressure contours from NOAA NOMADS GRIB2 data, rendered with high-fidelity Marching Squares contours. |

---

## Map Enhancements

- **Robinson projection** — an additional map projection option alongside Mercator (equirectangular) and Azimuthal
- **Single Azimuthal projection** — DE-centered azimuthal equidistant circle; the original HamClock only offered the dual-hemisphere variant
- **Interactive Zoom & Pan** — Use the mouse wheel to zoom (clamped 1x–10x) and left-drag to pan the map. Double-right-click to instantly reset zoom and pan settings.
- **POTA activator map pins** — lime-green pins on the map for active POTA activations (requires On The Air widget in a pane)
- **ADIF QSO map pins** — plots your logged QSOs from an ADIF file on the map (requires ADIF widget)
- **Satellite ground track** — the selected satellite's orbital path is drawn as an arc on the map (`showSatTrack`)
- **Beacon/widget-aware plotting** — beacon markers and Live Spots map pins are only drawn when the corresponding widget is actually in a pane's rotation list

---

## DX Cluster & Live Spots Enhancements

### Duplicate Spot Hiding

The DX Cluster panel can be configured to "Hide duplicates (one per call/band)". When enabled, only the most recent spot for a station on a given amateur band is displayed, significantly cleaning up the view during contests or pileups.

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
- **RSS ticker** — optional scrolling RSS feed display
- **CORS proxy** — configurable relay URL for browser builds; see [Glossary](Glossary.md#cors-proxy)

---

## UI & UX Improvements

- **K key highlight mode** — press K to draw cyan bounding boxes around every interactive region on screen with tooltip labels; hovering over a widget body now also shows the widget's description in a blue tooltip.
- **? Help Panel** — Press **?** (Shift+/) to open a full-screen scrollable help panel listing all keyboard shortcuts, mouse/touch controls, and a complete gallery of widgets with descriptions.
- **Large Kp number overlay** — current Planetary K-index rendered as a large color-coded number over the bar chart in the Aurora Graph widget (storm level colors)
- **Contest detail popup** — clicking a contest row in the Contests widget opens a detail panel with exchange, rules link, and category summary
- **On The Air / DX Cluster Refinements** — Band color legends added to ONTAPanel in double-height mode; Live Spots switches to efficient 1-column layout in double-height mode.
- **Marine Auto-Lookup** — The Marine widget now features a "Find Closest" button that automatically identifies and selects the nearest NOAA tide and buoy stations.
- **Global Timezone Picker** — A centralized timezone modal (accessible from Setup) allows setting a default timezone that is respected by the Time, Calendar, Big Clock, Contest, EME, and Greyline widgets.
- **Scrollable widget list in Setup** — the full widget checklist in Setup → Widgets scrolls with mouse wheel and `^`/`v` arrows when it overflows
- **Greyline DX scroll** — the Greyline DX widget list now scrolls with mouse wheel when more entities are active than fit in the pane
- **Background data fetching** — propagation calculations and news feed updates run in the background so the display stays smooth and responsive
- **QRZ premium callbook** — in addition to free Callook and HamDB lookups, QRZ.com XML API is supported for subscribers
- **Aux Clock timezone cycling** — click the Aux Clock widget to cycle through preset timezones (UTC, EST, CST, MST, PST, CET, JST, AEST). The selected timezone persists across restarts. Useful for running a local-time clock alongside a full-pane DX Cluster so meetings are not missed. Can also be set via API: `/set_config?aux_tz_offset=-5&aux_tz_label=EST`

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
