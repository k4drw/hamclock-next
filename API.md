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

---

## Debugging & Automation API

### `GET /debug/widgets`
Returns a JSON snapshot of all currently active UI widgets and their "semantic actions". This is the primary endpoint for building automated tests or smart remote controls.

### `GET /debug/click?widget=W&action=A`
Performs a semantic click on a specific widget action.
- `widget`: The ID of the widget (e.g., `SolarPanel`)
- `action`: The name of the action (e.g., `ToggleMode`)
- **Example**: `GET /debug/click?widget=SolarPanel&action=Cycle`

### `GET /debug/type?text=T`
Simulates typing a full string into the application.
- **Example**: `GET /debug/type?text=K4DRW`

### `GET /debug/keypress?key=K`
Alternative to `set_char`, optimized for lowercase key names.

---

## Proposed Future Endpoints

The following endpoints are suggested for future implementation to improve remote management:

- `GET /get_dx.txt`: Report current DX location and distance/bearing.
- `POST /set_config`: Allow updating configuration via JSON payload.
- `GET /status.json`: A unified endpoint returning SFI, SSN, current satellites, and active alerts.
- `GET /screenshot.png`: High-quality PNG capture (lossless).
- `POST /restart`: Software-initiated application restart.
- `GET /log`: Retrieve recent application logs.
