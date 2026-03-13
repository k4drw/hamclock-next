# Changelog

All notable changes to HamClock-Next are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [v1.0.0] — 2026-03-13

### Added
- **Heritage Release** — First stable 1.0.0 release.
- **100% Feature Parity** (82/82 features) with original HamClock by WB0OEW.
- **Dual-Hemisphere Azimuthal Projection** — Sidebar-by-side globes centered on DE and antipode.
- **High-Fidelity GribCloud Overlay** — 1440x721 layered cloud system from NOAA GFS.
- **Seamless Date Line Overlays** — Eliminates gaps at -180/180 longitude across all projections.
- **Manual Pane Rotation** — Navigation arrows on widgets for immediate pane cycling.
- **Dedicated WSJT-X UDP Port** — Separate configuration for digital mode spotting.
- **Aux Clock Timezone Cycling** — Quick-switch between preset zones by clicking the widget.

### Changed
- **Unified Side Panels** — Side panels (DE/DX Info) are now standard panes supporting all 45 widgets.
- **Standardized UI Tokens** — Full migration to `ThemeColors` tokens across all UI components.
- **Optimized Memory Management** — Automatic low-memory mode detection for 1GB devices.

### Fixed
- **Antimeridian Path Clipping** — Corrects streaks in sat/asteroid tracks crossing the Date Line.
- **Division-by-zero Guards** — Epsilon checks in Great Circle and coordinate transformation math.
- **Security Hardening** — Weak-pointer callbacks and SQL prepared statements throughout.
- **REST API Completion** — 79 endpoints verified and documented.

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

### Fixed (post-B04 pre-release fixes — 2026-03-09)
- **RigService.cpp** — `sscanf` format `%s` on `modeStr[32]` replaced with `%31s` to prevent buffer overflow on malformed CAT responses
- **RotatorService.cpp** — Added `if (!store_)` null-pointer guards at four `store_->get()` call sites (lines 120, 149, 225, 278) matching the existing guard at line 64; affected paths: `setPosition()`, `stopRotator()`, `pollLoop()` disconnect, auto-tracking loop
- **OrbitPredictor.cpp** — Added cap on `minutes` before `* 60` multiplication to prevent int32 overflow on pathological inputs
- **MapWidget.cpp** — Added epsilon guard (`fabs(dLon) > 1e-6`) at antimeridian path-crossing calculation in 6 locations (sat track, asteroid track, `renderGreatCircle` duplicate sites, and 3 path-rendering functions) to prevent div-by-zero when a point lies exactly on ±180°
- **UI theme colors** — Replaced hardcoded RGB values with `themes.*` tokens across 11 files: `TropoPanel.cpp`, `GreylineModal.cpp`, `ContestPanel.cpp`, `PresetsModal.cpp`, `StopwatchPanel.cpp`, `IonosondePanel.cpp`, `CountdownPanel.cpp`, `EMEToolPanel.cpp`, `ReminderPanel.cpp`, `SetupScreen.cpp` (ensures consistent Light/Dark/Glass appearance)
- **Framebuffer blanking** — `blankFramebuffer()` now sends `FB_BLANK_NORMAL` before `FB_BLANK_POWERDOWN` so display power actually cuts; `#ifdef` guards normalized across `BME280Provider.cpp` and `FccProvider.cpp`
- **REST-API.md** — Removed 10 stale endpoints not registered in WebServer.cpp; added `/set_mappos`; updated endpoint count to ~80
- **API.md** — Removed unregistered `/set_theme` entry; corrected legacy compatibility note
- **README.md** — Feature count 71 → 82; endpoint count "30+" → 79
- **feature_overview.md** — Feature count 71 → 82; endpoint count corrected; C++ standard clarified to "C++20 (C++17 for WASM)"
- **docs/wiki/Getting-Started.md** — CMake minimum version 3.16 → 3.18
- **docs/wiki/Home.md** — Widget count 44 → 45 (two occurrences)
- **CONTRIBUTING.md** — Broken link `docs/wiki/Building.md` → `docs/wiki/Getting-Started.md`

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
