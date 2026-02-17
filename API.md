# HamClock-Next API Documentation

HamClock-Next includes an embedded web server (defaulting to port 8080) that provides a live view and a REST-like API for remote control and debugging.

## Remote View & Interaction

### `GET /`
Provides a web-based live view of the HamClock screen. It supports mouse/touch interaction and keyboard input.

### `GET /live.jpg`
Returns the current screen as a JPEG image. Useful for lightweight monitoring.

---

## Remote Control API

### `GET /set_touch?rx=F&ry=F`
Simulates a mouse click/touch event at the specified normalized coordinates.
- `rx`: Horizontal position (0.0 to 1.0)
- `ry`: Vertical position (0.0 to 1.0)
- **Example**: `GET /set_touch?rx=0.5&ry=0.5` (clicks center of screen)

### `GET /set_char?k=S`
Simulates a keyboard event.
- `k`: A single character (e.g., `a`, `1`, `+`) or a named key:
  - `Enter`, `Backspace`, `Tab`, `Escape`, `Space`
  - `ArrowLeft`, `ArrowRight`, `ArrowUp`, `ArrowDown`
  - `Delete`, `Home`, `End`, `F11`
- **Example**: `GET /set_char?k=Enter`

### `POST /api/display/power`
Turns the display on or off.
- **Request (JSON)**: `{"state": "on"}` or `{"state": "off"}`
- **Fallback**: Also supports `GET /api/display/power?state=on`
- **Returns**: JSON with success status and method used.

### `GET /api/display/status`
Returns the current display power state and the control methodology being used (e.g., `vcgencmd`, `bl_power`).
- **Returns**: JSON with `power` ("on"/"off") and `method`.

---

## Reporting API

### `GET /get_config.txt`
Returns basic configuration fields in a plain-text format (HamClock compatible).
- Fields included: `Callsign`, `Grid`, `Theme`, `Lat`, `Lon`

### `GET /get_time.txt`
Returns the current UTC time in the standard HamClock format:
`Clock_UTC YYYY-MM-DDTHH:MM:SS Z`

### `GET /get_de.txt`
Returns current DE (Designated Entry) location information.
- Fields: `DE_Callsign`, `DE_Grid`, `DE_Lat`, `DE_Lon`

### `GET /get_dx.txt`
Returns current DX (target) location and path information.
- Fields: `DX_Grid`, `DX_Lat`, `DX_Lon`, `DX_Dist_km`, `DX_Bearing`
- Returns "DX not set" if no DX location is active.

### `GET /set_mappos?lat=F&lon=F[&target=S]`
Programmatically sets the DE or DX location on the map.
- `lat`: Latitude in degrees (-90 to 90)
- `lon`: Longitude in degrees (-180 to 180)
- `target`: `de` or `dx` (default: `dx`)
- **Returns**: JSON with `target`, `lat`, `lon`, `grid`
- **Example**: `GET /set_mappos?lat=40.7&lon=-74.0&target=dx`

---

## Debugging & Automation API

### `GET /debug/widgets`
Returns a JSON snapshot of all currently active UI widgets and their "semantic actions".
- **Enhanced Data**: Now includes a `data` field for each widget containing high-level state (e.g., current SFI, active watchlist hits, etc.).
- **Map Widget**: The `data` field includes DE/DX positions, sun position, satellite info, spot counts, and current tooltip text.

### `GET /debug/click?widget=W&action=A`
Performs a semantic click on a specific widget action.
- `widget`: The ID of the widget (e.g., `SolarPanel`)
- `action`: The name of the action (e.g., `ToggleMode`)
- **Example**: `GET /debug/click?widget=SolarPanel&action=Cycle`

### `GET /debug/performance`
Returns real-time performance metrics in JSON format.
- `fps`: Current frames per second.
- `running_since`: Application uptime in seconds.

### `GET /debug/health`
Returns a JSON map of background service statuses.
- Shows `ok` status, `lastError`, and `lastSuccess` timestamp for services like NOAA, PSK Reporter, etc.

### `GET /debug/logs`
Returns the recent internal application log buffer (last 500 entries) in JSON format.

### `GET /debug/store/set_solar?sfi=N&k=N&sn=N`
Allows manual injection of solar weather data for testing purposes.

### `GET /debug/watchlist/add?call=XY1ABC`
programmatically adds a callsign to the monitor watchlist.

### `GET /debug/type?text=T`
Simulates typing a full string into the application.
- **Example**: `GET /debug/type?text=K4DRW`

---

---

## Configuration Notes

### config.json Structure (02-17-2026)

Operators who manually edit `config.json` should be aware of the following structure changes introduced in the 02-17-2026 session:

- **Countdown timer fields** are now stored under a `countdown` section:
  ```json
  { "countdown": { "enabled": true, "duration_sec": 300, "label": "QSO Timer" } }
  ```
- **Web password** is now stored under a `security` section:
  ```json
  { "security": { "web_password": "" } }
  ```
- Backward-compatibility fallback reads legacy flat keys on first load and migrates them automatically.

No GPS-specific REST endpoints exist yet. GPS provider status is internal; DE auto-set from gpsd is pending.

---

## Proposed Future Endpoints

The following endpoints are suggested for future implementation to improve remote management:

- `GET /screenshot.png`: High-quality PNG capture (lossless).
- `POST /restart`: Software-initiated application restart.
- `POST /factory_reset`: Clear all settings and caches.

