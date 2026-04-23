// WidgetDescriptions.cpp — human-readable one-line help text for every widget.
//
// Call populateWidgetDescriptions() once at app startup (from DashboardContext
// constructor) after all REGISTER_WIDGET static initializers have run.
// Descriptions are kept to ~80 characters so they fit comfortably in tooltips.
// There are 64 registered widget typeIds (some require config keys or hardware).

#include "WidgetRegistry.h"

void populateWidgetDescriptions() {
  auto &r = WidgetRegistry::instance();

  // ── Space Weather ────────────────────────────────────────────────────────────
  r.setDescription("solar",
    "Solar flux, sunspot number, X-ray & proton flux, solar wind, and Kp index.");
  r.setDescription("sdo",
    "Live solar imagery from the Solar Dynamics Observatory in selectable wavelengths.");
  r.setDescription("aurora",
    "Global aurora forecast map from the NOAA OVATION model.");
  r.setDescription("aurora_graph",
    "Time-series graph of geomagnetic Kp index and aurora activity levels.");
  r.setDescription("drap",
    "D-Region Absorption Prediction — HF absorption map of current ionospheric conditions.");
  r.setDescription("solar_storm",
    "Current geomagnetic storm watch, warning, and alert status from NOAA SWPC.");
  r.setDescription("history_flux",
    "Historical solar flux (SFI) bar chart over the past 30 days.");
  r.setDescription("history_kp",
    "Historical planetary K-index (Kp) bar chart over the past 30 days.");
  r.setDescription("history_ssn",
    "Historical sunspot number (SSN) bar chart over the past 30 days.");
  r.setDescription("dst_index",
    "Disturbance Storm Time index — measures geomagnetic storm severity in real time.");
  r.setDescription("ionosonde",
    "Ionospheric sounding parameters (foF2, MUF, hmF2) from a remote ionosonde.");
  r.setDescription("noaa_spacewx",
    "NOAA Space Weather forecast text: geomagnetic, solar radiation, and radio conditions.");
  r.setDescription("spacewx_alerts",
    "Active NOAA Space Weather alerts and warnings as a scrollable list.");
  r.setDescription("kindex_trend",
    "K-Index trend bar chart with alert threshold indicator.");
  r.setDescription("sfi_trend",
    "Solar Flux Index (SFI) 30-day trend bar chart.");
  r.setDescription("solar_cycle",
    "Current solar cycle progression chart: predicted vs. observed sunspot numbers.");
  r.setDescription("solar_timeline",
    "Timeline of recent solar events: flares, CMEs, and geomagnetic storm onsets.");

  // ── Propagation ──────────────────────────────────────────────────────────────
  r.setDescription("band_conditions",
    "Color-coded HF band conditions (160m–10m) for short path, long path, and trans-polar.");
  r.setDescription("live_spots",
    "Real-time decoded signal spots from PSK Reporter or Reverse Beacon Network by band.");
  r.setDescription("ncdxf",
    "NCDXF/IBP international beacon schedule with the current beacon on air.");
  r.setDescription("voacap_dedx",
    "VOACAP short-path propagation prediction between DE and DX, by band and hour.");
  r.setDescription("tropo",
    "Tropospheric ducting forecast map showing enhanced VHF/UHF propagation opportunities.");
  r.setDescription("meteor",
    "Meteor scatter propagation windows based on current meteor shower activity.");

  // ── DX / Spots ───────────────────────────────────────────────────────────────
  r.setDescription("dx_cluster",
    "Live DX spots from a telnet DX cluster or WSJT-X UDP feed, scrollable list.");
  r.setDescription("greyline_dx",
    "DXCC entities currently in the greyline, sorted by minutes to peak offset.");
  r.setDescription("dx_peditions",
    "Upcoming and active DXpeditions with dates, entity, and band/mode information.");
  r.setDescription("on_the_air",
    "Active POTA and SOTA activations worldwide with frequency and park/summit details.");
  r.setDescription("adif",
    "Displays recent QSOs from a local ADIF log file; auto-refreshes as log grows.");
  r.setDescription("callbook",
    "Callsign lookup showing name, QTH, grid square, and license class.");
  r.setDescription("watchlist",
    "Monitors specific callsigns in the DX cluster stream and highlights matches.");
  r.setDescription("alerts",
    "Triggered alerts based on watchlist hits or band activity thresholds.");
  r.setDescription("greyline_windows",
    "Daily greyline opening and closing times for configured DXCC entities.");
  r.setDescription("dxcc_progress",
    "DXCC award progress tracker — worked vs. confirmed entities by band/mode.");
  r.setDescription("was_progress",
    "Worked All States (WAS) tracking grid showing status (worked/confirmed) for all 50 US states.");

  // ── Tracking ─────────────────────────────────────────────────────────────────
  r.setDescription("satellite",
    "Next-pass predictions for selected satellites with AOS/LOS time and elevation.");
  r.setDescription("moon",
    "Moon phase, rise/set times, azimuth/elevation, and EME Doppler shift.");
  r.setDescription("eme_tool",
    "EME (Earth-Moon-Earth) link budget tool showing window, polarity, and path loss.");
  r.setDescription("asteroid",
    "Near-Earth asteroid close-approach table with distance and magnitude.");
  r.setDescription("gimbal",
    "Antenna gimbal/rotator readout showing azimuth and elevation from a connected controller.");

  // ── Timers ───────────────────────────────────────────────────────────────────
  r.setDescription("countdown",
    "Configurable countdown timer with a custom label and optional audio alert.");
  r.setDescription("stopwatch",
    "Lap-capable stopwatch with start, stop, and reset controls.");
  r.setDescription("reminders",
    "Recurring or one-shot reminders with custom text and alert sound.");

  // ── Clocks & Info ────────────────────────────────────────────────────────────
  r.setDescription("big_clock",
    "Full-pane UTC and local time display in large digits.");
  r.setDescription("callsign_clock",
    "Callsign display with UTC clock — ideal as a primary identification widget.");
  r.setDescription("clock_aux",
    "Auxiliary clock showing a second timezone alongside the main clock.");
  r.setDescription("world_clock",
    "Up to four configurable world clocks with city names and UTC offsets.");
  r.setDescription("calendar",
    "Current month calendar with today highlighted; scrollable by month.");
  r.setDescription("de_info",
    "DE (home station) info: callsign, grid, lat/lon, and sunrise/sunset times.");
  r.setDescription("dx_info",
    "DX (target) info: callsign, entity, grid, distance, bearing, and sunrise/sunset.");

  // ── Weather ──────────────────────────────────────────────────────────────────
  r.setDescription("de_weather",
    "Current conditions and forecast for the DE (home) location.");
  r.setDescription("dx_weather",
    "Current conditions and forecast for the DX (target) location.");
  r.setDescription("lightning",
    "Live lightning strike density map from the Blitzortung global network.");
  r.setDescription("hurricane",
    "Active tropical cyclone tracks in the Atlantic, Pacific, and Indian Oceans.");
  r.setDescription("marine",
    "Marine weather and vessel AIS traffic near the DE or a configurable position.");

  // ── Contests & Activities ────────────────────────────────────────────────────
  r.setDescription("contests",
    "Upcoming and active amateur radio contest schedule with mode and bands.");
  r.setDescription("forecast",
    "Contest and propagation forecast: band-by-band predicted conditions for the week.");

  // ── System / Utilities ───────────────────────────────────────────────────────
  r.setDescription("sys_info",
    "System diagnostics: CPU load, RAM, GPU VRAM usage, uptime, and network stats.");
  r.setDescription("rig_control",
    "Rig Control — displays frequency, mode, and S-meter from a connected transceiver.");
  r.setDescription("winlink",
    "Winlink gateway status and recent message traffic for the configured account.");
  r.setDescription("repeater_dir",
    "Repeater directory showing local repeaters by distance from DE.");
  r.setDescription("santa_tracker",
    "Santa's position on Christmas Eve — tracks the NORAD Santa radar in real time.");

  // ── Environment Sensors (BME280 / LTR329) ────────────────────────────────────
  r.setDescription("env_temp",
    "Local temperature reading from a connected BME280 sensor.");
  r.setDescription("env_pressure",
    "Barometric pressure from a connected BME280 sensor with trend arrow.");
  r.setDescription("env_humidity",
    "Relative humidity from a connected BME280 sensor.");
  r.setDescription("env_dewpoint",
    "Calculated dew point temperature from BME280 temperature and humidity readings.");
}
