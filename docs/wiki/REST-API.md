# HamClock-Next REST API

HamClock-Next provides a comprehensive REST API over HTTP for querying status, fetching current data, and remote-controlling the application. The API runs on the same port as the Live Web Interface (default: `8081`). There are **79 registered endpoints** across 7 functional groups.

## 1. Live Web & Streaming Endpoints
These endpoints provide access to the user interface remotely.
- `GET /` : Returns the Live Web Control UI HTML.
- `GET /live` : Returns the interactive live-view HTML page (requires `--live-web`).
- `GET /stream.mjpeg` : Continuous motion JPEG stream of the screen.
- `GET /live.jpg` (or `/get_capture`) : Returns a single JPEG snapshot.
  - Optional query parameter `seq={number}`: If provided, the server waits until the specified sequence number is reached before returning the frame. This ensures you get a *new* frame rather than a cached one.
  - If `seq` is omitted, the server automatically waits for the next captured frame (latest sequence + 1).
- `GET /live/touch?x={x}&y={y}&w={w}&h={h}&button={btn}` : Synthesizes a mouse click on the screen.
- `GET /live/key?k={key}` : Synthesizes a keyboard key press.
- `GET /live/mouse?x={x}&y={y}` : Synthesizes a mouse movement.
- `GET /live/wheel?y={delta}` : Synthesizes a mouse wheel input event.
- `GET /live/status` : Returns whether the live interactive interface is enabled.

## 2. Configuration & State API
These endpoints handle reading and writing application settings.
- `GET /api/config` : Returns core configuration data in JSON format.
- `GET /set_config?...` : Updates configuration parameters (e.g., `call`, `grid`, `lat`, `lon`, `theme`, `map_style`, `prop_overlay`, `wx_overlay`).
- `GET /set_rss?url={url}` : Sets the RSS news feed URL.
- `GET /set_mapcolor?theme={name}` : Changes the map background theme.
- `GET /set_projection?type={mercator|azimuthal}` : Changes the map projection.
- `GET /set_prop_overlay?type={none|muf|drap|aurora|...}` : Sets the map propagation overlay.
- `GET /set_wx_overlay?type={none|temp|rain|wind}` : Sets the map weather overlay.
- `GET /api/hub/fetch?url={B64_URL}&max_age={SEC}` : Proxies a fetch through the Master's cache (Master mode only).
- `GET /api/hub/dxcluster` : Returns recent spots from the Master's DX Cluster store.
- `POST /set_adif` : Uploads an ADIF log file body to the ADIFProvider.
- `GET /api/display/status` : Returns display uptime and power status.
- `POST /api/display/power` : Sets display power on/off (requires JSON body `{"state": "on"}`).
- `GET /set_displayOnOff?on|off` : Sets the display screen to turn on or off.
- `GET /set_screenlock?lock=on|off` : Enables or disables the screen touch lock.
- `GET /set_callsign?call={callsign}` : *Legacy Alias*: Updates the operator callsign (use `/set_config?call=...`).
- `GET /set_location?lat={lat}&lon={lon}` : *Legacy Alias*: Updates the local DE location (use `/set_config?lat=...`).
- `GET /set_theme?theme={dark|light|glass|default}` : *Legacy Alias*: Changes the UI theme (use `/set_config?theme=...`).
- `GET /set_metric?units={metric|imperial}` : Changes the unit measurement system.
- `GET /set_brightness?pct={0-100}` : Sets the display brightness level.
- `GET /set_newde?lat={lat}&lon={lon}` : Updates the origin DE location.
- `GET /set_newdx?lat={lat}&lon={lon}` : Updates the destination DX location.
- `GET /set_cluster?host={host}&port={port}&user={user}` : Configures the DX cluster settings.
- `GET /set_title?call={title}` : Sets the window title or callsign.

## 3. Panes & Widgets API
Controls the assignment and visualization of widgets in panes.
- `GET /api/widgets/available` : Returns a JSON array of all supported widgets.
- `GET /api/panes` : Returns JSON status of all 6 configurable panes (current widget, paused state, rotation list).
- `GET /api/panes/rotate?pane={0-5}` : Forces the specified pane to advance to its next widget.
- `GET /api/panes/rotate_all` : Forces all panes to advance.
- `GET /api/panes/pause?pane={0-5}` : Toggles the paused state of a specific pane.
- `GET /api/panes/pause_all?paused={0|1}` : Pauses or resumes all panes.
- `GET /api/panes/toggle?pane={0-5}&widget={id}` : Toggles a specific widget in or out of a pane's rotation list.
- `GET /set_pane?Pane1={widget1},...` : Set a rotation list of widgets per pane.
- `GET /get_pane.txt` : Plaintext status of assigned widgets for each pane.
- `GET /get_active_pane.txt` : Returns the currently visible widget name for each pane.
  - **Note**: This returns dynamic names. For example, if Pane 6 is in Satellite mode, it returns `Satellite` instead of `DX Info`.
- `GET /set_displayTimes?on={HH:MM}&off={HH:MM}&idle={mins}` : Schedule display wake/sleep times.
- `GET /set_mapcenter?lng={X}` : Changes the center longitude point of the map.
- `GET /set_panzoom?pan_x={X}&pan_y={Y}&zoom={Z}` : Sets map zoom level.
- `GET /set_rotation?pause|resume|next` : Pauses or resumes the widget rotation loop.
- `GET /set_rotator?state={stop|auto}&az={X}&el={X}` : Configure azimuth/elevation rotator.
- `GET /get_satellite.txt` : Gets the details for the currently active tracking satellite.
- `GET /set_satname?name={name}` : Change the actively tracked satellite.
- `GET /set_sattle?name={name}&t1={t1}&t2={t2}` : Insert your own custom satellite Tracking Elements.
- `GET /set_alarm?state={armed|off}&time={HH:MM}&utc={0|1}` : Sets an alarm timestamp.
- `GET /set_once_alarm?state={armed|off}&time={YYYY-MM-DDTHH:MM}` : Sets an alarm for a specific calendar date.
- `GET /set_stopwatch?reset|run|stop|countdown={mins}` : Controls the built-in stopwatch.

## 4. System Control & Time
Administrative system and timing actions.
- `GET /restart` : Gracefully exits the binary (usually assumes an external watch script will restart it).
- `GET /reboot` : Issues a `sudo reboot` OS execution block.
- `GET /set_time?ISO={timestamp}` : Injects an explicit or relative fake clock offset.
- `GET /set_demo?on={0|1}` : Turns on demonstration mode.
- `POST /api/reload` : Re-initializes UI using the latest configuration files.

## 5. Data Retrieval API (Legacy Space Weather & Logging)
Query internally gathered telemetry and spot data quickly.
- `GET /get_status.txt` : Returns the basic status (version, uptime, paused rotators).
- `GET /get_sensors.txt` : Returns basic connected temperature/hygrometer sensor readouts.
- `GET /get_memory.txt` : Returns VRAM usage bytes and CPU statistics.
- `GET /get_config.txt` : A brief plaintext readout of vital configuration keys.
- `GET /get_time.txt` : Gets the current internal simulated or real clock UTC.
- `GET /get_spacewx.txt` : Gets current solar data statistics (SN, SFI, A/K Indexes, etc).
- `GET /get_sys.txt` : Gets current system resource statistics.
- `GET /get_contests.txt` : Gets the currently loaded Ham Contests active this week.
- `GET /get_dxspots.txt` : Returns real-time DX spots parsed from logs.
- `GET /get_livespots.txt` : Returns currently tracked PSK/WSJT live spots.
- `GET /get_livestats.txt` : Returns statistics over currently tracked live spots by band.
- `GET /get_satellites.txt` : Retrieves an array of all available satellite TLE objects.
- `GET /get_ontheair.txt` : Gets spots of active POTA/SOTA on-the-air activity.
- `GET /get_dxpeds.txt` : Details listed upcoming DX Peditions.
- `GET /get_de.txt` : Returns current DE (home station) details: callsign, grid, lat/lon, bearing, and DXCC info.
- `GET /get_dx.txt` : Retrieves coordinate vectors between User DE and target DX location.

## 6. Propagation API
Get data necessary to render map overlays.
- `GET /api/propagation/voacap` : Returns VOACAP propagation map metadata based on parameters for use with open-hamclock-backend.
- `GET /api/propagation/muf_rt` : Returns near-real-time Maximum Usable Frequency map metadata from KC2G.

## 7. Interactive & Diagnostics
These endpoints synthesize clicks or manage diagnostic processes.
- `GET /set_touch?x={x}&y={y}` : Sends a touch event directly to the logical UI layer.
- `GET /set_wheel?y={delta}` : Synthesizes a mouse wheel input event.
- `GET /debug/keypress?key={key}` : Synthesizes a keyboard keypress natively.
- `GET /debug/click?widget={name}&action={name}` : Force the UI to evaluate an action click by name.
- `GET /debug/widgets` : Outputs a dictionary of every tracked UI Action target rectangle in memory.
- `GET /debug/watchlist/add?call={call}` : Adds a call to the custom alerting Watchlist.
- `GET /debug/store/set_solar` : Manually mutate the tracked solar metrics for testing.
- `GET /debug/performance` : Retrieves raw FPS and uptime telemetry.
- `GET /debug/logs` : Points developers where to find system log locations.
- `GET /debug/health` : Gives success/warning/failures for all running background networking tasks.
