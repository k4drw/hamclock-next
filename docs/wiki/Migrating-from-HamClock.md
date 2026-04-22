# Migrating from Original HamClock

This page is for users of **Elwood Downey's original HamClock** (WB0OEW) who are switching to HamClock-Next. It documents what changed, what stayed the same, and where to find familiar features.

---

## In Memoriam

**Elwood Downey, WB0OEW** — Silent Key, 29 January 2026.

Elwood created HamClock and maintained it for many years as a labor of love for the amateur radio community. HamClock-Next is a community continuation of his work, carrying forward his vision while modernizing the platform. His name and callsign are honored in this project.

73 de K4DRW

---

## What Stayed the Same

- **Same core purpose** — amateur radio dashboard with space weather, DX cluster, propagation maps, and astronomical data
- **Same data sources** — NOAA SWPC, [PSK Reporter / RBN / WSPR](Glossary.md#psk-reporter--rbn--wspr), DX cluster telnet, [POTA / SOTA](Glossary.md#pota--sota), DX Peditions from ng3k.com
- **Same map concept** — Mercator and Azimuthal (great circle) projections, gray line, propagation overlays
- **Same widgets (mostly)** — Solar, DX Cluster, Band Conditions, Moon, Aurora, DRAP, SDO, and many more are present
- **K key** — interactive element discovery with cyan highlights and tooltips (same behavior, expanded coverage)

---

## What Changed

### Rendering Engine: SDL2 (not X11 / framebuffer-only)

The original HamClock was built on a custom framebuffer/X11 rendering stack. HamClock-Next uses [SDL2](Glossary.md#sdl2), which enables:

- **Browser support via [WebAssembly](Glossary.md#webassembly-wasm)** — run in any modern browser, no installation
- **Native windows on Linux, macOS, and Windows** with proper window management
- Hardware-accelerated rendering on supported systems

If you ran the original HamClock in framebuffer mode on a Raspberry Pi, HamClock-Next works the same way.

---

### Pane Rotation — Manual Cycling, Configurable Interval, and Presets

The original HamClock already supported multiple widgets per pane with auto-rotation (one instance of each widget across all panes). HamClock-Next extends this with:

- **Manual cycling** — left/right arrows on each pane let you step through its widget list immediately; no need to wait for the timer
- **Configurable rotation interval** — the dwell time is now user-settable (default 30 seconds); the original used a fixed interval
- **Sync rotation** — all panes can advance simultaneously as a coordinated "page turn"
- **Presets (widget/map groups)** — save a named snapshot of all six pane rotations plus map overlays and propagation settings; recall with one click (★ button)
- **Side panel mode** — the left column can be switched to a full-height DX Cluster, On The Air, or Live Spots panel

---

### Configuration: JSON (not binary / EEPROM)

The original HamClock stored configuration in a binary format or EEPROM (on embedded platforms). HamClock-Next stores all configuration in a [human-readable JSON file](Glossary.md#json).

You can edit it with any text editor, copy it between machines, and version-control it with git.

---

### New Widgets Not in Original HamClock

| Widget          | Description                                   |
| --------------- | --------------------------------------------- |
| **Asteroid**    | Next 5 close Earth approaches (JPL data)      |
| **Ionosonde**   | Live ionospheric sounding data                |
| **Lightning**   | Real-time lightning strike activity           |
| **Meteor**      | Meteor scatter activity and upcoming showers  |
| **Tropo**       | Tropospheric ducting forecast                 |
| **Presets**     | Save/recall complete dashboard configurations |
| **Sys Info**    | System CPU, memory, network, uptime           |
| **Solar Storm** | Geomagnetic storm watch/warning/alert         |

---

### Live Spots — In-Widget Band Selection

The original HamClock supported [RBN, PSK Reporter, and WSPR](Glossary.md#psk-reporter--rbn--wspr) as Live Spots sources, and offered band filtering — but both were accessed through a separate widget config menu (Ok/Cancel dialog). In HamClock-Next, **bands are toggled by clicking them directly in the widget**; no config menu needed. Source selection is also surfaced inline rather than requiring a separate dialog.

---

### Presets System

The **★ (star) button** in the Time Panel opens the new Presets modal. Save a named configuration snapshot that captures pane layouts, overlays, map style, and propagation settings. Switch between saved presets instantly.

This replaces the need to manually reconfigure everything when switching between operating modes (e.g., contest vs. casual DX).

---

### On The Air — All / POTA / SOTA Filter

The original On The Air widget showed all POTA and SOTA activations together with no way to separate them. HamClock-Next adds a **filter chip** at the top of the widget to show All activations, POTA only, or SOTA only. The active filter is persisted in configuration (`ontaFilter`).

---

## Feature Mapping: Original → Next

| Original HamClock Feature                | HamClock-Next Equivalent                                                |
| ---------------------------------------- | ----------------------------------------------------------------------- |
| Multiple widgets per pane, auto-rotation | Same, plus: manual cycling arrows, configurable interval, sync rotation |
| Fixed rotation interval                  | User-configurable rotation interval (default 30 s)                      |
| No manual pane advance                   | Left/right arrows advance pane immediately, reset timer                 |
| Binary/EEPROM config                     | JSON configuration file                                                 |
| X11 / framebuffer rendering              | SDL2 (native + WASM browser), X11/framebuffer/Windows/Mac/iOS/Android   |
| Live Spots band filter in config menu    | Band filter toggled by clicking bands directly in the widget            |
| No presets                               | Presets modal (★ button) — widget + map groups                          |

---

## What Is Not Yet in HamClock-Next

HamClock-Next is a work in progress. Some original HamClock features may not yet be implemented or may have reduced functionality.
If you find a feature gap, please open an issue on the project repository.

---

## First Launch — Automatic Settings Import

If you have used the original HamClock on the same machine, HamClock-Next can import your settings automatically on first launch.

It looks in two places for your old settings file:
- `~/.hamclock/eeprom`
- `/home/pi/.hamclock/eeprom`

If it finds one, it copies your callsign, grid, location, and other compatible settings into the new JSON format — so you do not need to re-enter them. The Setup screen will already show your information.

If no old settings are found, the Setup screen opens blank as usual.

---

## Migration Steps

1. Install or build HamClock-Next (see [Getting Started](Getting-Started.md))
2. On first launch, your old settings are imported automatically if HamClock was installed on the same machine (see above). Otherwise, enter your callsign, grid, and location in the Setup screen.
3. Open each pane's widget picker (click top strip of pane) and add your preferred widgets
4. Configure DX cluster in Setup → DX Cluster tab
5. Set up propagation overlay and band/mode/power to match your operating style
6. Save your configuration as a Preset (★ button) so you can recall it later
7. Optionally configure QRZ credentials for callbook lookups
