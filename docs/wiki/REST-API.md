# HamClock-Next REST API

> **For programmers and automation users.** This page documents the HTTP control interface for scripting, integration with logging software, or building custom dashboards. If you just want to use HamClock-Next, you don't need this page — see [Getting Started](Getting-Started.md) instead. Unfamiliar terms? See the [Glossary](Glossary.md).

HamClock-Next provides a comprehensive [REST API](Glossary.md#rest-api) over HTTP for querying status, fetching current data, and remote-controlling the application. The API runs on the same port as the Live Web Interface (default: `8081`). There are over **100 registered endpoints** available.

### Thread-Safe Command Queuing

API commands are queued and applied on the next render frame. This prevents display glitches when automation scripts send rapid commands — you can send commands quickly without breaking the display.

---

## 1. Live Web & Streaming Endpoints

These endpoints provide access to the user interface remotely.

- `GET /` : Returns the Live Web Control UI HTML.
- `GET /live` : Returns the interactive live-view HTML page (requires `--live-web`).
- `GET /stream.mjpeg` : Continuous motion JPEG stream of the screen.
- `GET /live.jpg` (or `/get_capture`) : Returns a single JPEG snapshot.
  - Optional query parameter `seq={number}`: If provided, the server waits until the specified sequence number is reached before returning the frame. This ensures you get a *new* frame rather than a cached one.
  - If `seq` is omitted, the server automatically waits for the next captured frame (latest sequence + 1).
- `GET /live/touch?x={x}&y={y}&button={btn}` : Synthesizes a mouse click on the screen. Coordinates are transformed from physical window pixels to internal 800x480 logical space.
- `GET /live/key?k={key}&ctrl={0|1}&shift={0|1}` : Synthesizes a keyboard key press with optional modifiers.
- `GET /live/mouse?x={x}&y={y}` : Synthesizes a mouse movement.
- `GET /live/wheel?y={delta}` : Synthesizes a mouse wheel input event.
- `GET /live/status` : Returns whether the live interactive interface is enabled.

---

## 2. Configuration & State API

These endpoints handle reading and writing application settings.

- `GET /api/config` : Returns core configuration data in JSON format.
- `GET /get_config.json` : Redirects to `/api/config`.
- `GET /set_config?...` : Updates configuration parameters. Key fields: `call`, `grid`, `theme`, `map_style`, `prop_overlay`, `wx_overlay`, `night_lights`, `use_metric`, `dx_enabled`, `rbn_enabled`, `aux_tz_offset`, `aux_tz_label`.
- `GET /set_rss?enabled={0|1}` : Enables or disables the RSS news feed.
- `GET /set_mapcolor?key={key}&color={#RRGGBB}` : Overrides a specific theme color (e.g., `map_bg`, `callsign_bg`).
- `GET /set_dx?lat={lat}&lon={lon}&call={call}&grid={grid}` : Updates target DX location info.
- `GET /api/hub/fetch?url={B64_URL}&max_age={SEC}` : Proxies a fetch through the Master's cache (Master mode only).
- `GET /api/hub/dxcluster` : Returns recent spots from the Master's DX Cluster store as JSON.
- `POST /set_adif` : Uploads an ADIF log file body.
- `GET /api/display/status` : Returns display uptime, FPS, and power status.
- `GET /set_displayOnOff?on|off` : Sets the display screen power.
- `GET /set_mappos?lat={lat}&lon={lon}&target={de|dx}` : Set DE or DX position.
- `GET /set_newde?lat={lat}&lon={lon}` : Update DE location.
- `GET /set_newdx?lat={lat}&lon={lon}` : Update DX location.
- `GET /set_title?call={title}` : Update callsign label.

### Presets API

- `GET /api/presets` : Returns a list of all saved preset names.
- `GET /api/presets/apply?index={n}` : Applies the nth preset.
- `GET /api/presets/save?name={name}` : Saves the current state as a new preset.
- `GET /api/presets/delete?index={n}` : Deletes a preset.

---

## 3. Panes & Widgets API

Controls the assignment and visualization of widgets in panes.

- `GET /get_capabilities` : Returns a JSON dictionary of all available widgets, projections, themes, and overlays. Use this to populate external control UIs.
- `GET /api/widgets/available` : Returns a JSON array of specific widget metadata (id, display name, scrollable).
- `GET /api/panes` : Returns JSON status of all 6 configurable panes.
- `GET /set_pane?pane{0-5}={id1,id2}` : Set a rotation list of widgets for a specific pane.
  - **Note**: Commands are queued and applied on the next render frame to ensure stability.
- `GET /api/panes/rotate?pane={0-5}` : Advance a pane to the next widget in its list.
- `GET /api/panes/rotate_all` : Advance all panes.
- `GET /api/panes/pause?pane={0-5}` : Toggle paused state.
- `GET /api/panes/pause_all?paused={0|1}` : Global pause/resume.
- `GET /api/panes/toggle?pane={0-5}&widget={id}` : Toggle a widget in/out of a pane's rotation.
- `GET /api/panes/expand?pane={1-6}` : Expands a pane to double-height (equivalent to clicking the title bar).
- `GET /api/panes/collapse` : Collapses any expanded pane.
- `GET /get_pane.txt` : Plaintext status of assigned widgets for each pane.
- `GET /get_active_pane.txt` : Returns the currently visible widget ID for each pane.
- `GET /set_panzoom?pan_x={X}&pan_y={Y}&zoom={Z}` : Directly set map pan/zoom.
- `GET /set_sdo_options?wave={X}&rot={0|1}&pfss={0|1}&movie={0|1}` : Configure Solar widget (SDO) options.

---

## 4. Physical Layout & Coordinates

Use these endpoints to determine exactly where elements are on the screen (useful for OCR or automated screen capture cropping).

- `GET /get_pane_rect?pane={1-6}` : Returns JSON with `x, y, w, h` (logical) and `px, py, pw, ph` (physical pixels) for the specified pane.
- `GET /get_timepanel_rect` : Returns physical coordinates of the Time Panel.
- `GET /get_rss_rect` : Returns physical coordinates of the RSS Banner.

---

## 5. System Control & Time

- `POST /api/reload` : Re-initializes UI from configuration.
- `GET /api/fonts` : Returns a list of system-available fonts for the configuration UI.
- `GET /set_time?ISO={timestamp}` : Sets a relative fake clock offset.

---

## 6. Live Vectors Overlay

- `GET /api/live/vectors` : Returns a complex JSON object containing:
  - Current map projection type and logical map rectangle.
  - DE and DX coordinates.
  - All recent DX Cluster spots (lat/lon/call/freq).
  - All recent Live Spots (RBN/PSK) receiver coordinates.
  - Used by the `/live` web page to draw a high-fidelity vector overlay on top of the MJPEG stream.

---

## 7. Data Retrieval (Telemetry)

- `GET /get_spacewx.txt` : Current solar data (SN, SFI, K, etc).
- `GET /get_sys.txt` : Current system resources (CPU, Memory, Uptime).
- `GET /get_build.txt` : Version, architecture, and compile-time features.
- `GET /get_env.txt` : Operating system environment variables.
- `GET /get_sensors.txt` : Raw data from connected hardware sensors (BME280, GPS).
- `GET /get_contests.txt` : Active ham contests list.
- `GET /get_dxspots.txt` : Recent DX Cluster spots (plaintext).
- `GET /get_dxpeds.txt` : Recent and upcoming DX Peditions list (plaintext).
- `GET /get_livespots.txt` : Recent Live Spots.
- `GET /get_satellites.txt` : Full array of trackable satellites (JSON).
- `GET /get_ontheair.txt` : Recent POTA/SOTA spots.
- `GET /get_de.txt` : DE location and DXCC info.
- `GET /get_dx.txt` : DX location and bearing/distance.
- `GET /get_stopwatch.txt` : Current status of the stopwatch (running state and time).
- `GET /debug/logs` : Returns the last 100 lines of application logs.

---

## 8. Propagation & Solar Data

- `GET /api/propagation/voacap` : Raw VOACAP reliability/MUF map for the current configuration.
- `GET /api/propagation/muf_rt` : Near-real-time MUF map data.
- `GET /api/solar/sdo` : Information about available SDO wavelengths and state.
