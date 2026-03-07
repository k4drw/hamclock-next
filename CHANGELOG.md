# Changelog

All notable changes to HamClock-Next are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased — v1.0.0]

Planned stable release following B04 validation.

---

## [v1.0B04] — 2026-03-07

### Added
- **Aux Clock timezone cycling** — click the widget to cycle through preset timezones: UTC, EST(-5), CST(-6), MST(-7), PST(-8), CET(+1), JST(+9), AEST(+10). Persisted via `auxClockTzOffset`/`auxClockTzLabel` in config. API: `/set_config?aux_tz_offset=N&aux_tz_label=LABEL`
- **Greyline DX** added to Widgets.md category table (widget count corrected to 45)
- **`/get_de.txt`** documented in REST-API.md (route count corrected to 79)
- `AuroraGraphPanel.getName()` returns "Aurora Graph" (was "Widget" base fallback)
- `AuroraGraphPanel.onResize()` added
- `GreylineDXPanel.onMouseWheel()` with bounded scroll (`scrollOffset_` / `maxScroll_`)
- `ReminderPanel.onResize()` added (clears stale hover zones on geometry change)
- Aux Clock timezone section in `docs/wiki/Configuration.md`
- B04 entries in `docs/wiki/New-Features.md`

### Changed
- Widget count in Widgets.md and gallery caption: 44 → 45
- REST-API.md intro now states 79 endpoints

---

## [v1.0B03] — 2026-03-06

### Added
- **Dual-hemisphere azimuthal map projection** — two side-by-side azimuthal equidistant circles (DE-centered and antipodal) selectable from Map View menu
- **Wiki screenshot automation** — `/get_capture?seq=N` sequence logic enables `capture_wiki_screenshots.sh` to reliably capture fresh frames for docs
- Live Web Control UI overhaul with tabbed setup, watchlist, presets

### Fixed
- SDL_MOUSEWHEEL routing — wheel events now correctly route to active widget
- Bounded scroll on AlertsPanel, ForecastPanel, RepeaterPanel, WinlinkPanel, HurricanePanel
- Wiki/writeup accuracy corrections (widget names, pane numbering)
- Windows and WASM build compatibility restored
- `/get_capture` returning 0-byte files (seq fix)
- Map surgical endpoints `/set_mappos`, `/set_projection`, `/set_prop_overlay`
- DX cluster aggressive reconnection and rate limiting
- Hub mode timeout increased; DX cluster rate limits refined

---

## [v1.0B02] — 2026-02-28

### Added
- Presets system — save/load named dashboard configurations (pane widgets + overlays)
- DisplayPower overhaul with brightness glob detection and `/api/display/power` endpoint
- WX/Pressure overlay refactor with improved Marching Squares rendering
- RPM package for OpenSuSE Tumbleweed

### Fixed
- SetupScreen Appearance tab overflow on 1024×600 displays
- HistoryPanel value and time axis positioning
- DX Cluster column clip rects for long callsigns
- DX Cluster click bleed fix (left-edge clicks no longer open adjacent pane selector)
- Live configuration reload from web UI

---

## [v1.0B01] — 2026-02-14

### Added
- Initial public beta release
- Full SDL2/C++20 rewrite of original HamClock by WB0OEW (dedicated to his memory; Silent Key 29 January 2026)
- 82/82 feature parity with original HamClock
- Browser/WASM build via Emscripten
- Windows x64 cross-build via dockcross
- 44 selectable widgets across 6 categories
- 6-pane layout with per-pane rotation lists
- Embedded REST API (79 endpoints)
- JSON configuration with auto-migration from earlier beta keys
- Live Web Control UI with MJPEG stream and click/key injection
- Satellite tracking via libpredict v2
- VOACAP and near-real-time MUF propagation overlays
- Night lights, cloud, and WX pressure map overlays
- DX Cluster with telnet and WSJT-X UDP support
- ADIF log viewer
- Asteroid close-approach tracker (Minor Planet Center / JPL)
- Greyline DX entity panel
- Reminder panel with FCC license expiry auto-check
- Winlink gateway and RepeaterBook directory panels
- Hamlib rigctld and rotctld integration
