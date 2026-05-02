# Changelog

All notable changes to HamClock-Next are documented here.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [v1.6.0] — TBD

### Added
- **QSO Rate Tracker Widget** — sparkline chart of QSOs per hour for the past 12 hours with peak and total stats, sourced from local ADIF log.
- **Solar Flare Event Log Widget** — scrollable list of recent X-ray flares (class B–X) from NOAA SWPC with peak times and durations; fetches from dedicated endpoint every 15 minutes.
- **Greyline DX Spots Filter** — filters live DX cluster spots to show only those near the grey line (terminator window ±N degrees).
- **Band Advisor Widget** — real-time propagation status table for HF bands (80m, 40m, 20m, 15m, 10m) based on K-index and SFI; later consolidated into Band Conditions widget.
- **6m Band Support in Band Conditions** — extended Band Conditions widget to include 6m band with appropriate SFI/K-index thresholds.
- **LoTW Auto-Sync Widget** — automatic ADIF-to-LoTW upload integration with live activity tracking
- **Zone Heatmap Widget** — visual DXCC zone contact distribution
- **WAS Progress Widget** — US state contact tracker with map overlay
- **WAC Radar Widget** — 6-segment pie chart visualization (NA, SA, EU, AF, AS, OC) for continent-level award tracking
- **DX ATNO Alerts** — notify on rare DXCC entity spots matching ADIF needs
- **Custom RSS Feed Support** — user-configurable content in News widget
- **Enhanced Solar/Space Weather Widget** — 9 propagation overlays (MUF, reliability, TOA, DRAP, Aurora, etc.)
- **K-Index Alert Threshold** — configurable K-index trigger for notifications
- **DX Cluster Band Legend Click-To-Filter** — click band color to filter spots
- **DX Cluster Sub-Band Mode Badges** — visual CW/SSB/FT8/RTTY/WSPR indicators
- **SFI 30-Day Trend Chart** — solar flux index historical graph
- **VOACAP 6m Band Support** — propagation overlays for 50 MHz band
- **Horizontal Current Band Cursor** — frequency indicator on frequency axis
- **Live Spot Map Integration** — ON-THE-AIR spots overlay on map widget
- **DX Info Manual Entry** — enter DX callsigns directly via a centered modal dialog (triggered by clicking the widget); features auto-uppercasing and `api.hamdb.org` (CallbookProvider) lookup for accurate location resolution for hams who have moved; widget border highlights amber when the callsign appears in live spots (30s auto-fade)
- **Global Volume Control** — adjustable audio levels (0-100) for alarms and TTS; dedicated slider in Setup -> Identity tab
- **DE Station Status Preset** — new built-in preset highlighting award tracking (DXCC, WAS, Grid, WAC, Zone Heatmap) and QSO management (LoTW Sync, ADIF Tracking); factory preset deletion tracking ensures user-deleted presets don't reappear on app updates
- **Local Propagation Gauge** — compact map overlay at the map bottom showing real-time MUF, LUF, and recommended HF bands for the DE station; toggled via Map View menu; preference persisted as `showLocalPropGauge` in config.
- **Cache Statistics in SysInfoPanel** — displays texture, vertex, and command buffer usage stats for debugging performance on "Master Hub" (centralized) deployments.

### Changed
- **MCP Server Documentation** — Enhanced onboarding context for new widget contributors: MCP JSON now documents 50+ WidgetDeps fields (stores, providers, core resources) with usage patterns; expanded gotchas section from 1 → 6 entries covering REGISTER_WIDGET two-step requirement, MemoryMonitor::destroyTexture pattern, StatusCache optimization, DashboardContext lifecycle; added base class selection guidance (Widget vs ListPanel) to widget_scaffolding. Project-local hc-new-widget skill now includes Base Class Selection section, Debugging Patterns (common failure modes), rate limiting/backoff patterns, and enhanced References. MCP README now includes 5-step Newcomer Quick Start path. AI agents and humans adding widgets can complete tasks without grepping the codebase.
- **Wiki Documentation** — 100% v1.6 feature documentation coverage; 26 content gaps fixed across 15 pages including new sections for LoTW Auto-Sync, Award Trackers, Direct DX Lookup, and Global Volume Control.
- **DX Panel UI**
 — active status indicator, clickable expeditions, theme-aware colors
- **DXInfo Display** — DX local time via ZoneDetect library, bearing/distance tooltips
- **Propagation Overlay Auto-Switch** — smart band selection on frequency tune
- **LoTW & Clublog Panels** — theme color integration, text rendering cache for performance
- **ADIF Award Tracking Engine** — extended `ADIFStats` and `ADIFProvider` to track worked/confirmed continents, CQ zones, and ITU zones (parses `CQZ` and `ITUZ` tags)
- **Callbook Integration Service** — new `CallbookProvider` for asynchronous location resolution via external APIs
- **Polar Rendering Primitives** — new `RenderUtils` methods for `drawPie` and `drawArcOutline` supporting advanced circular UI elements
- **VOACAP DE-DX Timeline** — reoriented x-axis so the leftmost column is always the current UTC hour ("now at origin"); time flows right with 24-hour wrap-around; "now" cursor restored at left edge as a vertical white line for spot/band intersection visualization; legend items centered under graph columns; <10% reliability swatch changed from black to dark grey (64,64,64) for dark-theme contrast.

### Fixed
- **FlareProvider Wiring** — Fixed structural error in DashboardContext.cpp where FlareProvider was accessed via AppContext parameter instead of DashboardContext member; changed `ctx.flareProvider` to `flareProvider` at initialization and periodic-fetch sites. Flare widget now correctly initializes and fetches NOAA data.
- **Widget Font Readiness** — Added `fontMgr_.ready()` checks to QSO Rate Tracker, Band Advisor, Solar Flare Log, and Greyline Spots widgets to prevent rendering with uninitialized fonts at startup.
- **Band Conditions Widget Simplification** — Removed verbose "How it works" explanation panel from BandConditionsPanel; layout now shows compact band | day | night table. Deleted duplicate BandAdvisorPanel widget (functionality consolidated into Band Conditions).
- **Theming and Text Input Standards** — audit of 91 theme-aware and 17 text-input panels found 5 non-compliant instances (SetupScreen_Appearance dimTimeInput/brightTimeInput using textDim instead of border; DXInfo manual entry using hard-coded color, string input instead of TextInput class). All fixes applied; standards enforced across all UI components.
- **Presets Modal UX** — expanded visible rows from 5 to 8, added alphabetical sorting, fixed deletion tracking integration
- **GPU Texture Leaks** — SetupScreen, tooltip rendering, MemoryMonitor VRAM accounting drift
- **GPU Memory Stability** — FontManager LRU pruning on Raspberry Pi (low-memory mode)
- **MCP Diagnostic Tools** — fixed diagnose_memory endpoint polling, new_feature_checklist macro reference, removed 7 stale parity tools from docs
- **Propagation Overlay Sync** — band/overlay state consistency across panel updates
- **DX Cluster Crash** — Windows telnet socket NULL handling, spot sort order, band filter interaction
- **Windows SAPI Volume Reset** — explicitly manages SAPI voice levels to prevent volume resetting to system defaults on startup or speech events
- **Marine Widget Update** — immediate WTTR.in data fetch on load (no startup delay)
- **Network Timeout Stability** — 45s socket timeout for NASA/JPL/FCC/NOAA endpoints
- **Provider Async Use-After-Free (SIGSEGV)** — LoTWActivityProvider, ClublogProvider, and LoTWProvider refactored to inherit `std::enable_shared_from_this`; async fetch callbacks now capture `std::weak_ptr` and verify object lifetime via `lock()` before execution; DashboardContext manages these providers via `std::shared_ptr`. Eliminates crash during dashboard destruction with in-flight network requests.
- **Setup Services Tab** — fixed field focus order (QRZ → LoTW → Keys → Clublog), missing LoTW and Clublog bindings in `getActiveInput()`, `clublogApiKeyRect_` assignment gap, and mouse hit-testing across all 7 input fields; Tab key now cycles all fields in visual row order.
- **Setup Screen Layout** — resolved layout instability and reformatting issues in the Setup screen.
- **C++ Safety Audit** — resolved multiple safety hazards (lifetime, threading, etc.) identified during a comprehensive audit:
    - **Async Callback Safety** — `CallbookProvider` refactored to use robust pointer captures for nested asynchronous fetches.
    - **Web Server Thread Safety** — audited `WebServer` route handlers to ensure consistent mutex locking during multi-threaded data access.
    - **Network Thread Stability** — verified `NetworkManager` detached thread safety and `inflight_` counter logic to prevent use-after-free during shutdown.
- **Memory Stability** — resolved DX Cluster heap churn and a memory leak in the weather overlay rendering path.
- **Service Credential Persistence** — fixed issue where service credentials (LoTW, Clublog, etc.) were not persisted correctly; ensured Web API parity for remote configuration.
- **Core Stability Sweep** — comprehensive refactor of core application event handlers and service error handling:
    - **RAII Migration**: Converted 20+ custom event handlers in `DashboardContext` from raw `delete` pointers to `std::unique_ptr`, eliminating potential memory leaks.
    - **Provider Error Handling**: Established Network Callback Policy banning blanket `catch (...)` blocks. Refactored `NOAAProvider` and `ForecastProvider` to catch `std::exception`, log errors via `LOG_E`, and maintain valid UI state on failures.
    - **UI Layout Constants**: Added global UI layout constants to `Constants.h` to eliminate magic numbers and improve maintainability.
    - **Defensive Guards**: Hardened `SpaceWeatherPanel` with null-pointer guards for data stores and font catalogs.
- **Hub Master Network Optimization** — resolved critical memory usage issues on Hub Masters handling concurrent client requests:
    - **Streaming Responses**: Replaced `res.set_content` with `res.set_content_provider` in `/api/hub/fetch`, moving response body into lambda to stream directly to socket, eliminating 10MB+ string copies per request.
    - **Cache Metadata Preservation**: Modified `NetworkManager::fetchAsync` to preserve cache metadata (etag/lastModified) when entries go stale, enabling correct Conditional GET (304 Not Modified) behavior and preventing unnecessary full re-downloads.
    - **Proxy Force-Refresh**: Updated client proxy logic to pass `max_age=0` when `force=true`, ensuring fresh data fetches when requested.
- **Hub Master RAM Optimization** — finalized memory stability for Hub Masters to eliminate RAM ballooning during multi-client restarts:
    - **SharedString Buffers**: Replaced `std::string` with `SharedString` (`std::shared_ptr<const std::string>`) throughout NetworkManager stack, enabling multiple concurrent clients to stream from the exact same shared buffer (N*Size → 1*Size RAM usage).
    - **Extended RAM Cache**: Increased `MAX_RAM_BYTES` to 50MB and enabled RAM-caching of large payloads (>512KB) for Hub Masters, serving sequential client requests from memory instead of forcing redundant disk I/O and allocations.
    - **Zero-Allocation 304s**: Implemented early metadata validation in `/api/hub/fetch` to return 304 Not Modified without triggering disk reads if cache is fresh.
    - **Verification**: Hub Master RAM stabilized at ~350-380MB regardless of client restart patterns or concurrent map requests.

---

## [v1.5.0] — 2026-04-12

### Added
- **Thread-safe REST API** — API commands are queued and applied on the next render frame, eliminating display glitches when automation scripts send rapid commands.
- **DX Cluster Duplicate Hiding** — option to show only the most recent spot per callsign/band, cleaning up cluster view during contests.
- **Glossary** — new plain-language reference page defining JSON, REST API, Hamlib, Telnet, CORS proxy, and other terms for non-technical users.
- **PDF User Manual** — automated CI generation of `HamClock-Next-Manual.pdf` with cover sheet and release notes, attached to each GitHub release.

### Changed
- **Wiki overhaul** — all documentation pages updated for v1.5.0; platform/rendering sections rewritten to remove software-architecture jargon for the ham radio audience.
- **Configuration.md** — added plain-English intro explaining JSON format and config file location on each OS.
- **REST-API.md** — added "for programmers" audience callout at the top.

---

## [v1.4.0] — 2026-04-11

### Added
- **Native CCIR/VOACAP Engine** — high-fidelity HF propagation model directly ported from VOACAP Fortran, replacing the simplified PropEngine approximation with accurate, multi-path reliability and MUF calculations.
- **Global Timezone Support** — choose a dashboard-wide UTC offset in the setup screen to localize time across all panels, including TimePanel, Calendar, and VOACAP charts.
- **ONTAPanel Band Legend** — added a persistent band color legend to the "On The Air" widget when displayed in double-height mode (SidePanel Panes 5/6).
- **DX Local Time** — displays the active DX station's local time (HH:MM UTC±N) via an asynchronous background lookup service.
- **Interactive Map Zoom & Pan** — implemented mouse-wheel zooming and click-drag panning with automated coordinate transformation across all map projections.
- **DX Cluster Band Filter** — click any band label in the cluster's color legend to filter spots to that band; click again to clear.
- **Marine Auto-Setup** — when the Marine widget is added with no station configured, it automatically finds and saves the nearest NOAA tide station and NDBC buoy to your QTH.
- **Configurable Antenna Gain** — set target dBi (default 3 dBi) in VOACAP options (web + native UI) to tune reliability models to specific station hardware.
- **CCIR Coefficient Tools** — included scripts and automation for parsing and refreshing embedded ionospheric data from raw NOAA/CCIR ASCII sources.
- **World Clock Widget** — displays up to 4 configurable timezone clocks; each slot has a label, active toggle, and adjustable UTC offset (±30 min; Shift for ±1 hr). Configuration is accessed via an in-widget gear icon.

### Changed
- **Propagation Colormaps** — "Vibrant" is now the default; "Muted" is now a desaturated pastel palette for improved legibility in high-contrast environments.
- **Map Great-Circle Paths** — all great-circle overlays (Spots, ONTA, Selection) now use 250 segments for smooth curves at any zoom level; selection paths adopt the band color of the selected spot.
- **Map Reset Gesture** — reset is now triggered by right-mouse double-click, freeing left-click double-click from accidental resets during panning.
- **Pane 4 Restrictions** — centralized enforcement of compatible widgets for the small top-right pane to prevent layout collisions.
- **Live-Web UI** — overhauled event sequencing to restore full mouse interaction for web-based remote control.
- **Map View Menu** — Apply button uses the success (green) color for visual clarity.
- **LiveSpotPanel 1-Column Layout** — optimized the "Live Spots" widget to use a single-column list when in double-height mode, improving readability in the SidePanel.
- **Application Uptime** — updated the `TimePanel` display to track the uptime of the HamClock process (`hamclock-next`) instead of the underlying operating system.

### Fixed
- **DX Timezone Deadlock** — resolved a critical application freeze caused by re-locking the location mutex during asynchronous data delivery.
- **ONTAPanel, WatchlistPanel, AsteroidPanel** — removed hard row caps that silently dropped excess entries; all three panels now scroll with the mouse wheel.
- **DX Peditions Panel** — removed 10-row cap; full list now shown with scroll support.
- **Custom Font Visibility** — fixed a bug where the "Map View" button label would disappear when using certain symbol-heavy custom fonts.
- **Antimeridian Sat Tracks** — fixed visual artifacts and "streaks" in satellite ground tracks when the map is centered on non-zero longitudes.
- **Marine Widget** — fetches updated data immediately after saving new station or buoy IDs; no restart required.
- **Moon Widget** — reduced backing circle radius by 1 px to eliminate the visible halo; backing color updated to deep navy.
- **N-Hop MUF Model** — corrected F2 MUF and absorption calculations for long-path (>20,000 km) geometry using an N-hop model instead of hardcoded 1/2-hop logic.
- **VOACAP DE-DX Timeline** — chart now uses a fixed UTC midnight-to-midnight axis with a real-time "now" cursor; eliminates "split window" issues at the UTC boundary.
- **VOACAP DE-DX Widget** — isolated to always show the DE↔DX short path, decoupled from the map's long-path overlay setting.
- **Reliability Legend** — updated the 0% swatch to black/transparent to match the map's skip-zone rendering.
- **Memory Safety** — fixed a Use-After-Free risk during application shutdown by ensuring the WebServer thread stops before dashboard context destruction.
- **Sensor Shutdown Responsiveness** — `LTR329Provider` and `DXClusterProvider` now use interruptible condition-variable waits instead of bare `sleep_for` calls; `stop()` returns immediately rather than blocking for the full poll interval.
- **GPU Memory Stability** — capped the `FontManager` volatile text cache and implemented LRU pruning to prevent GPU buffer (BO) exhaustion during long-term operation on low-memory hardware (RPi 3B).
- **GPU Texture Leaks** — resolved critical per-frame leaks in `WorldClockPanel` and hover-tooltip rendering paths (DashboardContext/EMEToolPanel) that caused OOM crashes on Linux/RPi. Corrected `MemoryMonitor` VRAM accounting for textures destroyed via standard SDL calls.
- **MemoryMonitor Accounting Sweep** — proactive audit replaced bare `SDL_DestroyTexture` calls (which bypass VRAM tracking) with `MemoryMonitor::destroyTexture` in `ClockAuxPanel` (hmTex_/secTex_, destructor + render + resize — 5 sites) and `AsteroidPanel` icon render loop; eliminated a per-frame GPU texture allocation in `ONTAPanel` chip-label measurement, replacing `renderText`+`destroyTexture` with a CPU-only `getLogicalWidth` call.
- **ListPanel Geometry** — standardized row and footer calculations in the base `ListPanel` to prevent overlap between scrollable content and legend footers (used by DX Cluster and ONTA panels).

---

## [v1.3.0] — 2026-04-03

### Added
- **K-Index Alert & Trend** — new widget showing a 24-hour Kp sparkline with color-coded severity zones and G-scale alert labels.
- **SFI 30-Day Trend** — long-term Solar Flux Index chart with band-viability reference lines at 70 / 100 / 150 / 200 SFU.
- **Space Weather SWPC Alerts** — scrollable live timeline of NOAA SWPC alerts (flares, geostorms, CME) with color-coded severity.
- **NOAA Severity Scales** — 3×4 table showing current and D+1 to D+3 forecast for R (Radio Blackouts), S (Solar Radiation), and G (Geomagnetic) scales.
- **Big Clock Widget** — high-visibility standalone clock with digital and analog faces, user-selectable color themes, UTC/Local toggle, and smooth sub-second hand animation.
- **DX Cluster DXCC "Needed" Markers** — spots are tagged **N** (New DXCC), **B** (New Band), or **W** (Worked/Unconfirmed) based on your local ADIF log.
- **DX Cluster Sub-Band Mode Badges** — intelligent CW / SSB / FT8 / FT4 / RTTY detection based on spot frequency.
- **DX Cluster Band Legend** — persistent color legend identifying sub-band modes; click a band to filter (precursor to v1.4 full filtering).
- **Greyline Windows Widget** — 24-hour timeline of ±30-minute sunrise/sunset windows for both DE and DX locations.
- **Watchlist Batch Input** — paste comma- or space-separated callsigns (up to 256 characters) directly into the Watchlist setup field.
- **Global Watchlist Notifications** — spot alerts for watched callsigns fire even when the Watchlist widget is not currently visible in any pane.
- **VOACAP Short/Long Path & Take-Off Angle** — added Short Path / Long Path selection and user-defined TOA for high-fidelity propagation prediction.
- **Solar Cycle 25 Progress** — tracks current cycle age, SSN trends, and projected time to solar maximum.
- **"Center on DE" Map Mode** — persistent setting that keeps the map centered on your home longitude across all projections.
- **NCDXF Band-Coded Beacon Markers** — beacon map markers are now color-coded by their active transmitting band.
- **Marine "Find Closest" Feature** — automatically identifies and selects the nearest NOAA tide station and NDBC buoy based on your QTH coordinates.
- **Dynamic Watchlist Sync** — watchlist changes via the web UI take effect immediately without a service reload.

### Changed
- **Map Controls** — anti-aliased geometry for map controls and menus; tooltips suppressed when a widget is expanded to full height.
- **VOACAP UI** — propagation settings grouped into a "VOACAP Options" submenu; settings synchronized with the remote web UI.
- **Audio Mute** — global mute setting synchronized across all notification buffers and TTS generators.

### Fixed
- **NOAA SWPC Data Format** — migrated all solar data parsers (Kp, SFI, Sunspot Number, Solar Wind, DST, X-Ray flux) from array-indexed to key-based JSON lookup, resolving "No Data" errors caused by upstream API format changes.
- **Thread Safety** — resolved race conditions and deadlocks in network, sensor, and dashboard shutdown sequences.
- **Version Comparison** — rewrote update checker using 32-bit bit-packing for deterministic comparison across beta and stable versions.
- **Network Timeouts** — enforced 10-second server timeouts to prevent slow clients from stalling the HamClock process.
- **Android** — keyboard auto-show/hide resolved; accidental selections during scroll suppressed.
- **iOS** — landscape-only orientation enforced to preserve dashboard aspect ratio.
- **Windows** — system font discovery and SysInfo crash on environments without thermal sensor zones.

---

## [v1.2.0] — 2026-03-31

### Added
- **Android Support** — official APK build workflow via Docker/GitHub Actions; native touch UX (scroll-gesture suppression, immersive landscape mode).
- **iOS Support** — CI/CD pipeline for iOS simulator builds; landscape orientation enforced; Apple framework bridging.
- **VOACAP DE-DX Widget** — hour-by-hour path reliability matrix showing 24-hour propagation forecasts for the active DE–DX path.
- **Solar Cycle 25 Tracker Widget** — real-time SSN trends, cycle phase classification, and time-to-maximum prediction.
- **Greyline Windows Widget** — 24-hour timeline visualizing ±30-minute sunrise/sunset windows for DE and DX locations.
- **DXCC Progress (Recent) Widget** — dynamic tracking of unique DXCC entities worked from the 100 most recent ADIF log entries.
- **Space Weather Alerts Widget** — live stream of NOAA SWPC alerts (X/M flares, geostorms, CME) with color-coded severity.
- **NOAA Severity Scales Widget** — instant visualization of current R / S / G scale values.
- **Standalone Satellite Widget** — dedicated satellite tracking widget placeable in any pane, decoupled from the DX Info panel.
- **LiveSpots Map Tooltips** — hovering over a PSK Reporter / RBN / WSPR spot on the map shows frequency, mode, and country name.
- **Astronomical Star Field** — Yale BSC subset (mag ≤ 4.5) rendered on the map background, correctly projected for Robinson and Azimuthal views.
- **Side Panel Full-Height Mode** — Panes 5 and 6 can be toggled to full height for scrollable list widgets (DX Cluster, Live Spots, Calendar).

### Changed
- **DX Cluster Marker Style** — updated to match original HamClock "CircleWithDot" bullseye; band-color coding applied to active spots.
- **NCDXF Beacons** — rendered as standard triangles on the map, matching original HamClock style.
- **CalendarPanel** — added "Today" and "Tomorrow" date headings for improved event legibility.
- **Widget Self-Registration** — eliminated centralized WidgetType enum and switch blocks; widgets now self-register via `REGISTER_WIDGET` macro for a modular, contributor-friendly architecture.
- **Setup Screen Pane Selection** — replaced abstract "Top 1–4" buttons with a visual pane diagram.

### Fixed
- **Thread Safety** — completed C++ concurrency audit; fixed 5 critical data races, TOCTOU hazards, and blocking joins.
- **Sensor Threads** — GPS, BME280, and LTR329 handlers now use interruptible condition variable waits instead of bare sleeps; improves shutdown responsiveness.
- **Observer Coordinates** — DXSatPane and GimbalPane now reliably load user-defined QTH coordinates on startup (was defaulting to 0,0).
- **Tooltip Ghost** — tooltips no longer persist when the cursor crosses pane boundaries.
- **Windows** — font discovery paths corrected; SysInfo crash resolved on systems without thermal sensor zones.
- **SpaceWxAlerts** — layout collision and flicker resolved.

---

## [v1.1.0] — 2026-03-20

### Added
- **Rig Control Widget** — live CAT control via Hamlib `rigctld`; compact mode shows frequency/mode, expanded mode adds frequency stepping, VFO A/B, RIT, split, and S-meter.
- **Solar Impact Timeline Widget** — 3-day Kp forecast displayed as an impact timeline.
- **Widget Maximize** — any pane can be expanded to temporarily fill the map area for a larger view.
- **Contest Mode Profile** — quick-swap contest layout preset with one click.
- **ONTA Geofenced Distance Filter** — filter POTA/SOTA activators by maximum distance (km) from your QTH.
- **Voice Alerts (TTS)** — native voice notifications via `flite` for Watchlist hits, solar flares, countdowns, and calendar events.
- **LTR329 Photosensor Driver** — automatic ambient-light dimming via I²C sensor on Raspberry Pi and similar hardware.
- **WSPR Heatmap Integration** — WSPR source data integrated into the propagation Heatmap engine (via db1.wspr.live).
- **New REST Endpoints** — `/get_capabilities`, `/get_config.json`, `/get_build.txt`, `/get_env.txt`, `/get_sensors.txt`, `/get_stopwatch.txt`, `/debug/logs` (in-memory ring buffer); `set_pane` extended with `solo` mode.
- **Glyph Font Fallback** — missing Unicode symbols now fall back to a bundled glyph font when a custom font is active.

### Changed
- **GreylineDXPanel** — added peak countdown timers and near-peak visual warnings.
- **CalendarPanel** — inline configuration, state persistence, ListPanel visual style, multi-line hover tooltips.
- **SolarStormPanel** — added CME arrival countdown display.
- **Architecture** — major decomposition of `SetupScreen.cpp` and `main.cpp` into focused modules; `ProviderBase` standardizes update intervals and thread lifetimes across all networking providers; `GraphHelper` deduplicates time-series rendering logic; `TimeUtils` standardizes date/time formatting.

### Fixed
- **Web Config** — missing handlers for latitude/longitude, audio muting, and font selection added to the web UI.
- **Moon and SDO Panels** — no longer render as invisible under light themes (paper background bleed-through resolved).
- **TTS Behavior** — startup greeting plays only once; voice alerts silent when screen is blanked.
- **Stability** — 4 dangling-pointer and Use-After-Free issues fixed in satellite tracking and WebServer teardown; MJPEG mutex deadlock resolved.
- **Heatmap Overlay** — fixed skipped data computations and persistence bugs.

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
