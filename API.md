# HamClock-Next REST API Documentation

HamClock-Next includes an embedded web server (defaulting to port 8080) that provides a live view and a REST API for remote control, monitoring, and debugging.

---

## 1. Remote View & Interaction

### `GET /`
Provides the primary web-based live view. This is an interactive application that supports mouse clicks and keyboard passthrough.

### `GET /live`
A simplified live viewer.

### `GET /stream.mjpeg`
Provides a live MJPEG video stream of the HamClock display.
- **Refresh Rate**: 1 FPS (default to conserve CPU)

### `GET /live.jpg`
Returns the current screen as a JPEG image.

### `GET /get_capture`
Alias for `/live.jpg`.

### `GET /live/touch?x=N&y=N[&button=0|1]`
Simulates a touch/click at the specified pixel coordinates (normalized to the 800x480 logical size).
- `x`: 0-799
- `y`: 0-479
- `button`: 0=left, 1=right

### `GET /live/key?k=S`
Simulates a key press. `k` can be a character or a special name (Enter, Escape, etc.).

### `GET /debug/keypress?key=N&mod=N`
Simulates a raw SDL keycode with modifiers.

---

## 2. Telemetry & Reporting

These endpoints return plain-text or JSON data about the current state.

### `GET /get_time.txt`
Returns `Clock_UTC YYYY-MM-DDTHH:MM:SS Z`.

### `GET /get_de.txt`
Returns DE (Designated Entry) location: Callsign, Grid, Lat, Lon.

### `GET /get_dx.txt`
Returns DX (Target) location: Callsign, Grid, Lat, Lon, Distance, Bearing.

### `GET /get_spacewx.txt`
Returns current space weather indices (SFI, SSN, Kp, etc.).

### `GET /get_sys.txt`
Returns system telemetry (Temperature, Uptime).

### `GET /get_sensors.txt`
Returns I2C/Environment sensor data (BME280) if available.

### `GET /get_memory.txt`
Returns memory diagnostic info (RSS, Texture count).

### `GET /api/config`
Returns the complete `AppConfig` as a JSON object.

---

## 3. Command & Control

### `GET /set_callsign?call=S`
Sets the station callsign.

### `GET /set_location?lat=F&lon=F` or `?grid=S`
Sets the DE (station) location.

### `GET /set_newdx?lat=F&lon=F` or `?grid=S`
Sets the DX (target) location.

### `GET /set_mappos?lat=F&lon=F&target=[de|dx]`
Sets DE or DX position.

### `GET /set_displayOnOff?[on|off]`
Controls the display power state.

### `POST /api/display/power`
Controls the display power state via JSON payload.
- **Body**: `{"state": "on"|"off"}`

### `GET /api/display/status`
Returns JSON with current power state and control method.

### `GET /set_brightness?n=N`
Sets screen brightness (0-100).

### `GET /set_theme?name=S`
Changes the UI theme (default, dark, glass).

---

## 4. Pane & Rotation Control

### `GET /set_pane?pane=N&widget=S`
Sets a specific pane to a specific widget.
- `pane`: 1-6
- `widget`: lowercase widget name (e.g., `solar`, `dx_cluster`, `marine`)

### `GET /get_pane.txt?pane=N`
Returns the name of the widget currently in the specified pane.

### `GET /set_rotation?[pause|resume]`
Pauses or resumes the automatic rotation of widgets in all panes.

### `GET /set_rotation?widget=S&pane=N`
Instantly jumps a specific pane to a widget and pauses rotation.

---

## 5. Propagation & Hub

### `GET /api/propagation/voacap?tx_lat=F&tx_lon=F&overlay_type=[muf|reliability|toa]`
Returns VOACAP propagation heatmap metadata.

### `GET /api/propagation/muf_rt`
Returns KC2G real-time MUF map metadata.

### `GET /api/hub/fetch?url=S`
(Client Mode) Proxies a request through the Master hub to avoid external rate limits.

---

## 6. Debug & Diagnostics

### `GET /debug/widgets`
Returns a detailed JSON map of all active widgets and their current data/actions.

### `GET /debug/click?widget=S&action=S`
Performs a "Semantic Click" on a widget action (e.g., `SolarPanel` / `Cycle`).

### `GET /debug/performance`
Returns real-time FPS and uptime.

### `GET /debug/health`
Returns status of all background data services.

### `GET /debug/logs`
Returns the recent internal log buffer.

### `GET /debug/store/set_solar?sfi=N&sn=N&k=N`
Injects manual solar data (for testing).

---

## Legacy Compatibility

HamClock-Next maintains 100% compatibility with all `GET /set_...` and `GET /get_...` endpoints from the original HamClock. Hardware controllers like the **Quadra** or **MegaClock** will work without modification.
