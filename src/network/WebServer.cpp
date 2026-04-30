#include "WebServer.h"
#include "../core/Logger.h"
#include "../ui/PaneContainer.h"
#include "../core/CalendarData.h"
#include "../ui/StopwatchPanel.h"
#include "NetworkManager.h"

#include <SDL.h>

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/StringUtils.h"
#include "../core/WatchlistStore.h"
#include "../core/WeatherData.h"
// Cap the httplib thread pool to 2 (default is max(8, hw_concurrency-1)).
// On RPi3B (4 cores) the default spawns 8 idle worker threads that
// continuously wake the scheduler, adding ~14% system CPU at idle.
// 2 threads is sufficient: one for the JPEG SSE stream, one for API calls.
#ifndef CPPHTTPLIB_THREAD_POOL_COUNT
#define CPPHTTPLIB_THREAD_POOL_COUNT 2
#endif
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../core/DisplayPower.h"
#include <future>
#include <sstream>

#include "../core/CPUMonitor.h"
#ifdef __linux__
#include <sys/time.h>
#endif
#include "../core/ActivityData.h"
#include "../core/BrightnessManager.h"
#include "../core/ContestData.h"
#include "../core/DXClusterData.h"
#include "../core/LiveSpotData.h"
#include "../core/SatelliteManager.h"
#include "../network/FrameCapture.h"
#include "../services/ADIFProvider.h"
#ifdef ENABLE_DEBUG_API
#include "../core/UIRegistry.h"
#endif
#include <iomanip>

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <iphlpapi.h>
#include <windows.h>
#endif

using namespace HamClock;

static bool isHostOnPrivateNetwork() {
#if defined(__linux__) || defined(__APPLE__)
  struct ifaddrs *ifap = nullptr;
  if (getifaddrs(&ifap) != 0)
    return true;
  bool foundNonLoopback = false, allPrivate = true;
  for (struct ifaddrs *ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;
    auto *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    uint32_t ip = ntohl(sa->sin_addr.s_addr);
    if (ip == 0x7F000001u || (ip >> 24) == 127)
      continue;
    foundNonLoopback = true;
    uint8_t a = (ip >> 24) & 0xFF, b = (ip >> 16) & 0xFF;
    if (!((a == 10) || (a == 172 && b >= 16 && b <= 31) ||
          (a == 192 && b == 168) || (a == 169 && b == 254)))
      allPrivate = false;
  }
  freeifaddrs(ifap);
  return !foundNonLoopback || allPrivate;
#else
  return true;
#endif
}

WebServer::WebServer(SDL_Renderer *renderer, AppConfig &cfg,
                     HamClockState &state, ConfigManager &cfgMgr,
                     std::shared_ptr<DisplayPower> displayPower,
                     std::atomic<bool> &reloadFlag,
                     std::shared_ptr<WatchlistStore> watchlist,
                     std::shared_ptr<SolarDataStore> solar,
                     std::shared_ptr<ContestStore> contests,
                     std::shared_ptr<DXClusterDataStore> dxc,
                     std::shared_ptr<LiveSpotDataStore> spots,
                     std::shared_ptr<CPUMonitor> cpu, int port)
    : renderer_(renderer),
      cfg_(&cfg),
      state_(&state),
      cfgMgr_(&cfgMgr),
      watchlist_(watchlist),
      solar_(solar),
      contests_(contests),
      dxc_(dxc),
      spots_(spots),
      cpu_(cpu),
      displayPower_(displayPower),
      reloadFlag_(&reloadFlag),
      port_(port) {}

WebServer::~WebServer() { stop(); }

void WebServer::start() {
#ifndef __EMSCRIPTEN__
  if (running_)
    return;
  const char *f = std::getenv("HAMCLOCK_FORCE_WEB");
  if (!(f && f[0] == '1') && !isHostOnPrivateNetwork())
    return;
  running_ = true;
  thread_ = std::thread(&WebServer::run, this);
#endif
}

void WebServer::stop() {
#ifndef __EMSCRIPTEN__
  running_ = false;
  if (svrPtr_)
    static_cast<httplib::Server *>(svrPtr_)->stop();
  if (thread_.joinable())
    thread_.join();
  svrPtr_ = nullptr;
#endif
}

void WebServer::run() {
#ifndef __EMSCRIPTEN__
  httplib::Server svr;
  svr.set_read_timeout(10, 0);
  svr.set_write_timeout(10, 0);
  svrPtr_ = &svr;
  svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
    std::string html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HamClock-Next Config</title>
  <style>
    :root { --green: #00e676; --dim: #333; --bg: #111; --card: #1a1a1a; }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { background: var(--bg); color: #eee; font-family: monospace; font-size: 14px; padding: 16px; }
    h1 { color: var(--green); margin-bottom: 16px; font-size: 1.2em; }
    .tabs { display: flex; gap: 4px; margin-bottom: 16px; }
    .tab { padding: 6px 14px; border: 1px solid var(--dim); cursor: pointer; background: var(--bg); color: #aaa; }
    .tab.active { border-color: var(--green); color: var(--green); }
    .panel { display: none; }
    .panel.active { display: block; }
    .card { background: var(--card); border: 1px solid var(--dim); padding: 12px; margin-bottom: 12px; }
    label { display: block; color: #aaa; margin-bottom: 4px; font-size: 0.85em; text-transform: uppercase; }
    input[type=text], input[type=number], input[type=password], select { width: 100%; padding: 6px 8px; background: #222; border: 1px solid var(--dim); color: #eee; font-family: monospace; font-size: 14px; margin-bottom: 10px; }
    input[type=range] { width: 100%; margin-bottom: 10px; accent-color: var(--green); }
    input[type=checkbox] { margin-right: 6px; accent-color: var(--green); }
    .chip { display: inline-block; padding: 3px 8px; border: 1px solid var(--dim); color: #aaa; cursor: pointer; font-size: 0.85em; }
    .chip.active { border-color: var(--green); color: var(--green); }
    .section-hdr { color: var(--green); font-size: 0.9em; margin-bottom: 8px; }
    input:focus { outline: 1px solid var(--green); border-color: var(--green); }
    button { padding: 8px 20px; background: #003300; border: 1px solid var(--green); color: var(--green); cursor: pointer; font-family: monospace; }
    button:hover { background: #004400; }
    .status-row { display: flex; justify-content: space-between; align-items: center; padding: 4px 0; border-bottom: 1px solid #222; }
    .status-row:last-child { border-bottom: none; }
    .ok { color: var(--green); }
    .err { color: #f44; }
    .dim { color: #666; font-size: 0.85em; }
    #msg { margin-top: 8px; color: var(--green); min-height: 1.2em; }
    #msg.err { color: #f44; }
    .pw-wrap { position: relative; display: flex; align-items: center; }
    .pw-wrap input { padding-right: 32px; margin-bottom: 0; }
    .pw-toggle { position: absolute; right: 8px; cursor: pointer; color: #888; font-size: 1.2em; user-select: none; }
    .pw-toggle:hover { color: var(--green); }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  </style>
</head>
<body>
  <h1>HamClock-Next )HTML";
    html += HAMCLOCK_VERSION;
    html += R"HTML(</h1>

  <div class="tabs" style="flex-wrap:wrap">
    <div class="tab active" onclick="showTab('identity')">Identity</div>
    <div class="tab" onclick="showTab('appearance')">Appearance</div>
    <div class="tab" onclick="showTab('status')">Status</div>
    <div class="tab" onclick="showTab('de-dx')">DE / DX</div>
    <div class="tab" onclick="showTab('network')">Network</div>
    <div class="tab" onclick="showTab('cluster')">Cluster</div>
    <div class="tab" onclick="showTab('radio')">Radio</div>
    <div class="tab" onclick="showTab('services')">Services</div>
    <div class="tab" onclick="showTab('brightness')">Brightness</div>
    <div class="tab" onclick="showTab('widgets')">Widgets</div>
    <div class="tab" onclick="showTab('widget-config')">Widget Config</div>
    <div class="tab" onclick="showTab('watchlist')">Watchlist</div>
    <div class="tab" onclick="showTab('update')">Update</div>
    <a class="tab" href="/live" target="_blank" style="text-decoration:none">Live View ↗</a>
  </div>

  <div id="identity" class="panel active">
    <div class="card">
      <label>Callsign</label>
      <input type="text" id="call" maxlength="12">
      <label>FCC FRN</label>
      <input type="text" id="call-frn" maxlength="10">
      <label>Grid Square</label>
      <input type="text" id="grid" maxlength="6">
      <label>Latitude</label>
      <input type="number" id="lat" step="0.0001" min="-90" max="90">
      <label>Longitude</label>
      <input type="number" id="lon" step="0.0001" min="-180" max="180">
      <label style="margin-top:4px"><input type="checkbox" id="gps-enabled"> Synchronize with GPS (gpsd)</label>
      <label><input type="checkbox" id="audio-muted"> Mute Audio / TTS</label>
      <label>Volume: <span id="vol-pct">100</span>%</label>
      <input type="range" id="audio-volume" min="0" max="100" value="100"
             oninput="document.getElementById('vol-pct').textContent=this.value">
      <label>Default Timezone</label>
      <select id="default-tz-preset" onchange="toggleDefaultTzCustom()">
        <option value="999|Local">Local (system time, DST-aware)</option>
        <option value="0|UTC">UTC</option>
        <option value="-5|EST">EST (UTC-5)</option>
        <option value="-6|CST">CST (UTC-6)</option>
        <option value="-7|MST">MST (UTC-7)</option>
        <option value="-8|PST">PST (UTC-8)</option>
        <option value="1|CET">CET (UTC+1)</option>
        <option value="9|JST">JST (UTC+9)</option>
        <option value="10|AEST">AEST (UTC+10)</option>
        <option value="custom|custom">Custom...</option>
      </select>
      <div id="default-tz-custom-fields" style="display:none; margin-top:6px">
        <label>UTC Offset (hours, -12 to +14)</label>
        <input type="number" id="default-tz-offset" min="-12" max="14">
        <label>Label (max 8 chars)</label>
        <input type="text" id="default-tz-label" maxlength="8">
      </div>
      <div class="grid-2" style="margin-top:8px">
        <div>
          <label>Callsign Color</label>
          <input type="color" id="call-fg" style="height:38px">
        </div>
        <div>
          <label>Callsign BG</label>
          <input type="color" id="call-bg" style="height:38px">
        </div>
      </div>
      <button onclick="saveConfig()" style="margin-top:8px">Save</button>
      <div id="msg"></div>
    </div>
  </div>

  <div id="appearance" class="panel">
    <div class="card">
      <label>Color Theme</label>
      <select id="theme">)HTML";
    for (const auto &t : getAvailableThemes()) {
      html += "<option value=\"" + t + "\">" + t + "</option>";
    }
    html += R"HTML(</select>
      <label>Map Style</label>
      <select id="map-style">
        <option value="nasa">NASA Blue Marble</option>
        <option value="topo">Topography</option>
        <option value="topo_bathy">Topo + Bathymetry</option>
      </select>
      <label>Projection</label>
      <select id="projection">
        <option value="equirectangular">Equirectangular</option>
        <option value="robinson">Robinson</option>
        <option value="azimuthal">Azimuthal</option>
        <option value="mercator">Mercator</option>
        <option value="dual_azimuthal">Dual Azimuthal</option>
      </select>
      <div style="display:flex; gap:12px; margin-bottom:10px; flex-wrap:wrap">
        <label><input type="checkbox" id="show-borders"> Borders</label>
        <label><input type="checkbox" id="show-beacons"> Beacons</label>
        <label><input type="checkbox" id="show-sattrack"> Sat Track</label>
        <label><input type="checkbox" id="center-map-on-de"> Center on DE</label>
      </div>
      <label>Grid Overlay</label>
      <select id="grid-mode">
        <option value="none">None</option>
        <option value="latlon">Lat/Lon</option>
        <option value="maidenhead">Maidenhead</option>
      </select>
      <label>Propagation Overlay</label>
      <select id="prop-overlay" onchange="toggleVoacapFields()">
        <option value="none">None</option>
        <option value="muf">MUF-RT</option>
        <option value="voacap">VOACAP</option>
        <option value="reliability">Reliability</option>
        <option value="toa">Time of Arrival</option>
        <option value="heatmap">Heatmap</option>
        <option value="drap">DRAP</option>
        <option value="aurora">Aurora</option>
      </select>
      <div id="voacap-settings" style="display:none; border:1px solid var(--dim); padding:10px; margin:10px 0; background:#1a1a1a">
        <div class="section-hdr">VOACAP Options</div>
        <label>Band</label>
        <select id="prop-band">
          <option value="80m">80m</option>
          <option value="40m">40m</option>
          <option value="30m">30m</option>
          <option value="20m">20m</option>
          <option value="17m">17m</option>
          <option value="15m">15m</option>
          <option value="12m">12m</option>
          <option value="10m">10m</option>
          <option value="6m">6m</option>
        </select>
        <label>Mode</label>
        <select id="prop-mode">
          <option value="SSB">SSB</option>
          <option value="CW">CW</option>
          <option value="FT8">FT8</option>
          <option value="WSPR">WSPR</option>
          <option value="AM">AM</option>
          <option value="RTTY">RTTY</option>
        </select>
        <label>Power (Watts)</label>
        <input type="number" id="prop-power" min="1" max="1500" value="100">
        <label>Take-Off Angle (deg)</label>
        <input type="number" id="prop-toa" step="1" min="0" max="90" value="3">
        <label>Path</label>
        <div style="display:flex;gap:14px;margin:4px 0 8px">
          <label style="display:flex;align-items:center;gap:5px;cursor:pointer"><input type="radio" name="prop-path" value="0" checked> Short</label>
          <label style="display:flex;align-items:center;gap:5px;cursor:pointer"><input type="radio" name="prop-path" value="1"> Long</label>
        </div>
        <label>Antenna Gain (dBi)</label>
        <input type="number" id="prop-ant-gain" min="-10" max="30" value="3">
      </div>
      <label>Weather Overlay</label>
      <select id="weather-overlay">
        <option value="none">None</option>
        <option value="wxmb">WX/Pressure</option>
        <option value="clouds_grib">Clouds (GFS)</option>
      </select>
      <label style="margin-top:10px"><input type="checkbox" id="night-lights"> Show Night Lights</label>
      <label><input type="checkbox" id="use-metric"> Use Metric Units</label>
      <label>MUF Real-time Opacity</label>
      <input type="range" id="muf-opacity" min="0" max="1" step="0.1" value="0.5">
      <div class="section-hdr" style="margin-top:16px">Font</div>
      <label>Font</label>
      <select id="font-path"></select>
      <button onclick="saveAppearance()" style="margin-top:10px">Save Appearance</button>
      <div id="app-msg"></div>
    </div>
    <div class="card">
      <label>Display Power Method</label>
      <select id="display-power-method" style="margin-bottom:10px">
        <option value="auto">Auto-detect</option>
      </select>
      <label>Display Power</label>
      <div style="display:flex; gap:10px">
        <button onclick="setPower('on')">ON</button>
        <button onclick="setPower('off')" style="border-color:#884444; color:#cc8888">OFF</button>
      </div>
      <div id="pwr-msg" class="dim" style="margin-top:8px"></div>
    </div>
  </div>

  <div id="status" class="panel">
    <div class="card">
      <div class="status-row"><span>UTC Time</span><span id="utc-time" class="dim">—</span></div>
      <div class="status-row"><span>Uptime</span><span id="uptime" class="dim">—</span></div>
      <div class="status-row"><span>FPS</span><span id="fps" class="dim">—</span></div>
    </div>
    <div class="card" id="services-card">Loading services...</div>
  </div>

  <div id="de-dx" class="panel">
    <div class="card">
      <strong style="color:var(--green)">DE</strong>
      <div id="de-info" class="dim" style="margin-top:8px">Loading...</div>
    </div>
    <div class="card">
      <strong style="color:var(--green)">DX</strong>
      <div id="dx-info" class="dim" style="margin-top:8px">Loading...</div>
    </div>
  </div>

  <div id="network" class="panel">
    <div class="card" id="cors-proxy-card">
      <label>CORS Proxy URL</label>
      <input type="text" id="cors-proxy-url" placeholder="/proxy/">
      <div class="dim" style="margin-bottom:10px">
        Prefix prepended to external API URLs in WASM builds.<br>
        Default <code>/proxy/</code> uses the bundled serve.py proxy.
      </div>
    </div>
    <div class="card">
      <div class="section-hdr">Local Data Hub</div>
      <label>Hub Mode</label>
      <select id="hub-mode" onchange="toggleHubFields()">
        <option value="Off">Off</option>
        <option value="Master">Master</option>
        <option value="Client">Client</option>
      </select>
      <div id="hub-client-fields" style="display:none">
        <label>Hub IP</label>
        <input type="text" id="hub-ip" placeholder="192.168.1.100">
        <label>Hub Port</label>
        <input type="number" id="hub-port" placeholder="8080">
      </div>
      <button onclick="saveNetwork()">Save</button>
      <div id="net-msg"></div>
    </div>
  </div>

  <div id="cluster" class="panel">
    <div class="card">
      <div class="section-hdr">DX Cluster</div>
      <label><input type="checkbox" id="dx-enabled"> Enable DX Cluster</label>
      <label><input type="checkbox" id="rbn-enabled"> Enable RBN (Reverse Beacon Network)</label>
      <label><input type="checkbox" id="dx-hide-dup"> Hide Duplicates</label>
      <label>Max Age (minutes)</label>
      <select id="dx-max-age">
        <option value="10">10 mins</option>
        <option value="20">20 mins</option>
        <option value="40">40 mins</option>
        <option value="60">60 mins</option>
      </select>
      <label>Host</label>
      <input type="text" id="dx-host" placeholder="dxusa.net">
      <label>Port</label>
      <input type="number" id="dx-port" min="1" max="65535">
      <label>Login Callsign</label>
      <input type="text" id="dx-login" placeholder="NOCALL">
      <label style="margin-top:4px"><input type="checkbox" id="dx-wsjtx"> Use WSJT-X (UDP) instead</label>
      <label>WSJT-X UDP Port</label>
      <input type="number" id="wsjtx-port" min="1" max="65535">
      <button onclick="saveCluster()">Save</button>
      <div id="cluster-msg"></div>
    </div>
  </div>

  <div id="radio" class="panel">
    <div class="card">
      <div class="section-hdr">Rig (rigctld)</div>
      <label>Host</label>
      <input type="text" id="rig-host" placeholder="localhost">
      <label>Port</label>
      <input type="number" id="rig-port" min="1" max="65535">
      <label><input type="checkbox" id="rig-autotune"> Auto-Tune on DX Spot click</label>
    </div>
    <div class="card">
      <div class="section-hdr">Rotator (rotctld)</div>
      <label>Host</label>
      <input type="text" id="rot-host" placeholder="localhost">
      <label>Port</label>
      <input type="number" id="rot-port" min="1" max="65535">
      <label><input type="checkbox" id="rot-autotrack"> Auto-Track Satellite</label>
      <label><input type="checkbox" id="rot-upover"> Upover Mode (Az flip)</label>
      <button onclick="saveRadio()">Save</button>
      <div id="radio-msg"></div>
    </div>
  </div>

  <div id="services" class="panel">
    <div class="card">
      <div class="section-hdr">QRZ Callsign Lookup</div>
      <label>Username</label>
      <input type="text" id="qrz-user">
      <label>Password</label>
      <div class="pw-wrap" style="margin-bottom:10px">
        <input type="password" id="qrz-pass" placeholder="(unchanged if blank)">
        <span class="pw-toggle" onclick="togglePW('qrz-pass')">👁</span>
      </div>
      <label>LoTW Callsign</label>
      <input type="text" id="lotw-call">
      <label>LoTW Password</label>
      <div class="pw-wrap" style="margin-bottom:10px">
        <input type="password" id="lotw-pass" placeholder="(unchanged if blank)">
        <span class="pw-toggle" onclick="togglePW('lotw-pass')">👁</span>
      </div>
      <label>Clublog API Key</label>
      <div class="pw-wrap" style="margin-bottom:10px">
        <input type="password" id="clublog-key">
        <span class="pw-toggle" onclick="togglePW('clublog-key')">👁</span>
      </div>
      <label>RepeaterBook Key</label>
      <div class="pw-wrap" style="margin-bottom:10px">
        <input type="password" id="rb-key">
        <span class="pw-toggle" onclick="togglePW('rb-key')">👁</span>
      </div>
      <label>Winlink Key</label>
      <div class="pw-wrap" style="margin-bottom:10px">
        <input type="password" id="wl-key">
        <span class="pw-toggle" onclick="togglePW('wl-key')">👁</span>
      </div>
    </div>
    <div class="card">
      <div class="section-hdr">Live Spots</div>
      <label>Source</label>
      <select id="spot-source">
        <option value="PSK">PSK Reporter</option>
        <option value="RBN">Reverse Beacon Network</option>
        <option value="WSPR">WSPR</option>
      </select>
      <label>Max Age (minutes)</label>
      <input type="number" id="spot-age" min="5" max="120">
      <label>Bands</label>
      <div id="band-chips" style="display:flex;flex-wrap:wrap;gap:6px;margin-bottom:10px"></div>
      <label style="margin-top:4px"><input type="checkbox" id="spots-of-de"> Show spots OF DE (vs spots BY DE)</label>
      <label><input type="checkbox" id="spots-use-call"> Filter by callsign (vs grid square)</label>
    </div>
    <div class="card">
      <div class="section-hdr">Toggles</div>
      <label><input type="checkbox" id="gps-enabled-serv"> GPS Auto-Location</label>
      <label><input type="checkbox" id="rss-enabled"> RSS News Banner</label>
      <label>POTA/SOTA Filter</label>
      <select id="onta-filter">
        <option value="all">All Spots</option>
        <option value="pota">POTA Only</option>
        <option value="sota">SOTA Only</option>
      </select>
      <label>Max Distance (km)</label>
      <input type="number" id="onta-max-dist" min="0" max="20000">
      <label>K-Index Alert Threshold</label>
      <input type="number" id="k-index-threshold" step="0.1" min="0" max="9">
      <button onclick="saveServices()">Save</button>
      <div id="services-msg"></div>
    </div>
  </div>

  <div id="brightness" class="panel">
    <div class="card">
      <label>Brightness: <span id="bright-pct">100</span>%</label>
      <input type="range" id="bright-level" min="10" max="100" value="100"
             oninput="document.getElementById('bright-pct').textContent=this.value">
      <label style="margin-top:4px"><input type="checkbox" id="bright-schedule"> Enable Day/Night Schedule</label>
      <label>Dim At (HH:MM)</label>
      <input type="text" id="dim-time" placeholder="22:00" maxlength="5">
      <label>Bright At (HH:MM)</label>
      <input type="text" id="bright-time" placeholder="06:00" maxlength="5">
      <button onclick="saveBrightness()">Save</button>
      <div id="bright-msg"></div>
    </div>
  </div>

  <div id="widgets" class="panel">
    <div class="card">
      <div class="section-hdr">Pane Rotation</div>
      <label>Rotation Interval (seconds)</label>
      <input type="number" id="rot-interval" min="5" max="3600">
      <label><input type="checkbox" id="sync-rot"> Synchronize pane rotation</label>
      
      <div class="section-hdr" style="margin-top:16px">Top Bar Panes</div>
      <div id="pane-config" style="display:grid; grid-template-columns: 1fr 1fr; gap:10px">
        <div>
          <label>Pane 1</label>
          <div id="pane0-list" class="widget-list"></div>
        </div>
        <div>
          <label>Pane 2</label>
          <div id="pane1-list" class="widget-list"></div>
        </div>
        <div>
          <label>Pane 3</label>
          <div id="pane2-list" class="widget-list"></div>
        </div>
        <div>
          <label>Pane 4</label>
          <div id="pane3-list" class="widget-list"></div>
        </div>
      </div>

      <div class="section-hdr" style="margin-top:16px">Side Panels</div>
      <label style="margin-bottom:10px"><input type="checkbox" id="full-height" onchange="toggleFullHeight()"> Full Height (Pane 5 takes both slots)</label>
      <div style="display:grid; grid-template-columns: 1fr 1fr; gap:10px">
        <div>
          <label>Pane 5 (Side L)</label>
          <div id="pane4-list" class="widget-list"></div>
        </div>
        <div id="pane5-container">
          <label>Pane 6 (Side R)</label>
          <div id="pane5-list" class="widget-list"></div>
        </div>
      </div>

      <div id="dx-sat-settings" style="margin-top:12px; padding:10px; background:#222; border:1px solid var(--dim)">
        <label>Identity Panel Mode</label>
        <select id="panel-mode" onchange="toggleSatSelect()" style="margin-bottom:0">
          <option value="dx">DX Info</option>
          <option value="sat">Satellite Tracking</option>
        </select>
        <div id="sat-select-container" style="display:none; margin-top:10px;">
          <label>Identity Satellite Tracker</label>
          <select id="selected-satellite" style="margin-bottom:10px"></select>
          <label>Standalone Widget Satellite</label>
          <select id="sat-widget-satellite" style="margin-bottom:0"></select>
        </div>
      </div>
      
      <button onclick="saveWidgets()" style="margin-top:16px">Save Widgets</button>
      <div id="widgets-msg"></div>
    </div>
  </div>

  <div id="widget-config" class="panel">
    <div class="card">
      <div class="section-hdr">Daily Alarm</div>
      <div class="grid-2">
        <div>
          <label>Alarm Time</label>
          <input type="time" id="alarm-time">
        </div>
        <div>
          <label>Mode</label>
          <select id="alarm-utc">
            <option value="0">Local</option>
            <option value="1">UTC</option>
          </select>
        </div>
      </div>
      <label><input type="checkbox" id="alarm-armed"> Alarm Armed</label>
    </div>

    <div class="card">
      <div class="section-hdr">SDO</div>
      <label>Wavelength</label>
      <select id="sdo-wavelength">
        <option value="211193171">Composite (211/193/171)</option>
        <option value="HMIB">Magnetogram</option>
        <option value="HMIIC">White Light (6173A)</option>
        <option value="0131">131A (Teal)</option>
        <option value="0193">193A (Brown)</option>
        <option value="0211">211A (Purple)</option>
        <option value="0304">304A (Red)</option>
        <option value="1600">1600A (Yellow)</option>
        <option value="1700">1700A (Pink)</option>
      </select>
      <div class="grid-2" style="margin-top:8px">
        <label><input type="checkbox" id="sdo-rotating"> Rotating</label>
        <label><input type="checkbox" id="sdo-pfss"> Show PFSS</label>
      </div>
    </div>

    <div class="card">
      <div class="section-hdr">Marine</div>
      <label>Tide Station ID</label>
      <div style="display:flex;gap:10px;align-items:center">
        <input type="text" id="marine-station" style="width:100px">
        <span id="marine-station-name" class="dim"></span>
      </div>
      <label style="margin-top:8px">Buoy ID</label>
      <input type="text" id="marine-buoy">
    </div>

    <div class="card">
      <div class="section-hdr">Big Clock</div>
      <div class="grid-2">
        <div>
          <label>Type</label>
          <select id="bc-digital">
            <option value="1">Digital</option>
            <option value="0">Analog</option>
          </select>
        </div>
        <div>
          <label>Time Format</label>
          <select id="bc-12h">
            <option value="0">24 Hour</option>
            <option value="1">12 Hour</option>
          </select>
        </div>
      </div>
      <div class="grid-2" style="margin-top:8px">
        <label><input type="checkbox" id="bc-utc"> UTC Mode</label>
        <label><input type="checkbox" id="bc-sec"> Show Seconds</label>
      </div>
      <div class="grid-2" style="margin-top:8px">
        <label><input type="checkbox" id="bc-date"> Show Date</label>
        <div>
          <label>Hue (0-359)</label>
          <input type="number" id="bc-hue" min="0" max="359">
        </div>
      </div>
    </div>

    <div class="card">
      <div class="section-hdr">Aux Clock</div>
      <label>Timezone</label>
      <select id="aux-tz-preset" onchange="toggleAuxCustom()">
        <option value="999|Local">Local (system time, DST-aware)</option>
        <option value="0|UTC">UTC</option>
        <option value="-5|EST">EST (UTC-5)</option>
        <option value="-6|CST">CST (UTC-6)</option>
        <option value="-7|MST">MST (UTC-7)</option>
        <option value="-8|PST">PST (UTC-8)</option>
        <option value="1|CET">CET (UTC+1)</option>
        <option value="9|JST">JST (UTC+9)</option>
        <option value="10|AEST">AEST (UTC+10)</option>
        <option value="custom|custom">Custom...</option>
      </select>
      <div id="aux-custom-fields" style="display:none; margin-top:6px">
        <label>UTC Offset (hours, -12 to +14)</label>
        <input type="number" id="aux-tz-offset" min="-12" max="14">
        <label>Label (max 8 chars)</label>
        <input type="text" id="aux-tz-label" maxlength="8">
      </div>
      <label>Mode</label>
      <select id="aux-star-mode">
        <option value="0">K</option>
        <option value="1">P</option>
        <option value="2">C</option>
      </select>
    </div>

    <div class="card">
      <div class="section-hdr">Other Display Options</div>
      <label>Idle Minutes (0=off)</label>
      <input type="number" id="idle-minutes" min="0" max="1440">
      <label><input type="checkbox" id="ltr329-auto-dim"> LTR329 Auto Dim</label>
      <label><input type="checkbox" id="prevent-sleep"> Prevent Screen Sleep</label>
    </div>

    <div class="card">
      <div class="section-hdr">Countdown Timer</div>
      <label>Event Label</label>
      <input type="text" id="countdown-label" placeholder="e.g. Field Day">
      <label>Target Date/Time (UTC)</label>
      <input type="text" id="countdown-time" placeholder="YYYY-MM-DD HH:MM">
    </div>

    <div class="card">
      <button onclick="saveWidgetConfig()">Save Widget Config</button>
      <div id="wcfg-msg"></div>
    </div>
  </div>

  <div id="watchlist" class="panel">
    <div class="card">
      <div class="section-hdr">Live Spot Watchlist</div>
      <div class="dim" style="margin-bottom:10px">Enter callsigns or keywords (one per line). Spots matching these will be highlighted.</div>
      <textarea id="watchlist-text" style="width:100%; height:200px; background:#222; border:1px solid var(--dim); color:#eee; font-family:monospace; padding:8px; margin-bottom:10px"></textarea>
      <button onclick="saveWatchlist()">Save Watchlist</button>
      <div id="watchlist-msg"></div>
    </div>
  </div>

  <div id="update" class="panel">
    <div class="card">
      <div class="section-hdr">System Information</div>
      <div class="status-row"><span>Version</span><span id="sys-version" class="dim">—</span></div>
      <div class="status-row"><span>Architecture</span><span id="sys-arch" class="dim">—</span></div>
      <div class="status-row"><span>Install Type</span><span id="sys-install" class="dim">—</span></div>
    </div>
    <div class="card">
      <div class="section-hdr">Update Instructions</div>
      <div id="update-instr" style="margin-bottom:10px"></div>
      <div id="update-cmd" style="color:var(--green); font-weight:bold"></div>
    </div>
  </div>

  <style>
    .widget-list { height: 120px; overflow-y: auto; border: 1px solid var(--dim); padding: 4px; background: #222; }
    .widget-item { font-size: 0.8em; padding: 2px 4px; cursor: pointer; border-bottom: 1px solid #333; }
    .widget-item:hover { background: #333; }
    .widget-item.active { color: var(--green); font-weight: bold; }
    .widget-item.disabled { color: #555; cursor: not-allowed; text-decoration: line-through; }
  </style>

  <script>
    // Tab navigation
    function showTab(name) {
      const ids = ['identity','appearance','status','de-dx','network','cluster','radio','services','brightness','widgets','widget-config','watchlist','update'];
      document.querySelectorAll('.tab').forEach((t,i) => t.classList.toggle('active', ids[i] === name));
      document.querySelectorAll('.panel').forEach(p => p.classList.toggle('active', p.id === name));
      if (name === 'appearance') { loadAppearance(); toggleVoacapFields(); }
      if (name === 'status') refreshStatus();
      if (name === 'de-dx') refreshDeDx();
      if (name === 'network') loadNetwork();
      if (name === 'cluster') loadCluster();
      if (name === 'radio') loadRadio();
      if (name === 'services') loadServices();
      if (name === 'brightness') loadBrightness();
      if (name === 'widgets') loadWidgets();
      if (name === 'widget-config') loadWidgetConfig();
      if (name === 'watchlist') loadWatchlist();
      if (name === 'update') loadUpdate();
    }

    function togglePW(id) {
      const el = document.getElementById(id);
      el.type = el.type === 'password' ? 'text' : 'password';
    }

    // Parse key-value text format ("Key   Value\n") — kept for Status/DE-DX tabs
    function parseKV(text) {
      const obj = {};
      text.split('\n').forEach(line => {
        const m = line.match(/^(\S+)\s+(.+)$/);
        if (m) obj[m[1]] = m[2].trim();
      });
      return obj;
    }

    // Load config on startup
    async function loadConfig() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('call').value = c.callsign || '';
        document.getElementById('call-frn').value = c.callsignFrn || '';
        document.getElementById('grid').value = c.grid || '';
        document.getElementById('lat').value = (c.lat !== undefined) ? c.lat : '';
        document.getElementById('lon').value = (c.lon !== undefined) ? c.lon : '';
        document.getElementById('gps-enabled').checked = !!c.gpsEnabled;
        document.getElementById('audio-muted').checked = !!c.audioMuted;
        const vol = c.audioVolume !== undefined ? c.audioVolume : 100;
        document.getElementById('audio-volume').value = vol;
        document.getElementById('vol-pct').textContent = vol;
        
        const tzOff = c.defaultTzOffset !== undefined ? c.defaultTzOffset : 0;
        const tzLbl = c.defaultTzLabel || 'UTC';
        const tzPresets = [{v:999,l:'Local'},{v:0,l:'UTC'},{v:-5,l:'EST'},
                           {v:-6,l:'CST'},{v:-7,l:'MST'},{v:-8,l:'PST'},
                           {v:1,l:'CET'},{v:9,l:'JST'},{v:10,l:'AEST'}];
        const match = tzPresets.find(p => p.v === tzOff && p.l === tzLbl);
        document.getElementById('default-tz-preset').value = match ? (tzOff+'|'+tzLbl) : 'custom|custom';
        if (!match) {
          document.getElementById('default-tz-offset').value = tzOff;
          document.getElementById('default-tz-label').value  = tzLbl;
        }
        toggleDefaultTzCustom();

        document.getElementById('call-fg').value = c.callsignColor || '#ffffff';
        document.getElementById('call-bg').value = c.callsignBgColor || '#000000';
        if (c.installType !== 'WASM') {
          document.getElementById('cors-proxy-card').style.display = 'none';
        }
      } catch(e) { setMsg('Failed to load config: ' + e, true); }
    }

    function toggleDefaultTzCustom() {
      const v = document.getElementById('default-tz-preset').value;
      document.getElementById('default-tz-custom-fields').style.display = v.startsWith('custom') ? 'block' : 'none';
    }

    async function loadFonts(selectedPath) {
      try {
        const r = await fetch('/api/fonts');
        const fonts = await r.json();
        const sel = document.getElementById('font-path');
        sel.innerHTML = '';
        fonts.forEach(f => {
          const opt = document.createElement('option');
          opt.value = f.path;
          opt.textContent = f.name;
          sel.appendChild(opt);
        });
        sel.value = selectedPath || '';
        if (sel.value !== (selectedPath || '')) sel.value = '';
      } catch(e) {}
    }

    async function loadAppearance() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('theme').value = c.theme || 'default';
        document.getElementById('map-style').value = c.mapStyle || 'nasa';
        document.getElementById('projection').value = c.projection || 'equirectangular';
        document.getElementById('show-borders').checked = !!c.showBorders;
        document.getElementById('show-beacons').checked = !!c.showBeacons;
        document.getElementById('show-sattrack').checked = !!c.showSatTrack;
        document.getElementById('center-map-on-de').checked = !!c.centerMapOnDe;
        
        let gridVal = 'none';
        if (c.showGrid) {
          gridVal = (c.gridType === 'maidenhead') ? 'maidenhead' : 'latlon';
        }
        document.getElementById('grid-mode').value = gridVal;

        document.getElementById('prop-overlay').value = c.propOverlay || 'none';
        document.getElementById('prop-band').value = c.propBand || '20m';
        document.getElementById('prop-mode').value = c.propMode || 'SSB';
        document.getElementById('prop-power').value = c.propPower || 100;
        document.getElementById('prop-toa').value = Math.round(c.propToa || 3);
        const pathVal = String(c.propPath || 0);
        document.querySelectorAll('input[name="prop-path"]').forEach(r => { r.checked = (r.value === pathVal); });
        document.getElementById('prop-ant-gain').value = (c.propAntGain !== undefined) ? c.propAntGain : 3;
        toggleVoacapFields();

        document.getElementById('weather-overlay').value = c.weatherOverlay || 'none';
        document.getElementById('night-lights').checked = !!c.mapNightLights;
        document.getElementById('use-metric').checked = !!c.useMetric;
        document.getElementById('muf-opacity').value = c.mufRtOpacity !== undefined ? c.mufRtOpacity : 0.5;
        await loadFonts(c.fontPath || '');

        const dpmSelect = document.getElementById('display-power-method');
        dpmSelect.innerHTML = '<option value="auto">Auto-detect</option>';
        if (c.displayPowerMethods) {
          c.displayPowerMethods.forEach(m => {
            if (m !== 'auto' && m !== 'none') {
              const opt = document.createElement('option');
              opt.value = m;
              opt.textContent = m;
              dpmSelect.appendChild(opt);
            }
          });
        }
        dpmSelect.value = c.displayPowerMethod || 'auto';

        document.getElementById('pwr-msg').textContent = 'State: ' + j.power + ' (' + j.method + ')';
      } catch(e) {}
    }

    function toggleVoacapFields() {
      const v = document.getElementById('prop-overlay').value;
      const show = (v === 'voacap' || v === 'reliability' || v === 'toa');
      document.getElementById('voacap-settings').style.display = show ? 'block' : 'none';
    }

    async function saveAppearance() {
      const theme = document.getElementById('theme').value;
      const mapStyle = document.getElementById('map-style').value;
      const projection = document.getElementById('projection').value;
      const showBorders = document.getElementById('show-borders').checked ? '1' : '0';
      const showBeacons = document.getElementById('show-beacons').checked ? '1' : '0';
      const showSatTrack = document.getElementById('show-sattrack').checked ? '1' : '0';
      const centerMapOnDe = document.getElementById('center-map-on-de').checked ? '1' : '0';
      const gridMode = document.getElementById('grid-mode').value;
      const propOverlay = document.getElementById('prop-overlay').value;
      const wxOverlay = document.getElementById('weather-overlay').value;
      const nl = document.getElementById('night-lights').checked ? '1' : '0';
      const mu = document.getElementById('use-metric').checked ? '1' : '0';
      const dpm = document.getElementById('display-power-method').value;
      
      const showGrid = (gridMode !== 'none') ? '1' : '0';
      const gridType = (gridMode === 'maidenhead') ? 'maidenhead' : 'latlon';

      const fontPath = document.getElementById('font-path').value.trim();
      const params = new URLSearchParams({
        theme, map_style: mapStyle, projection, show_borders: showBorders,
        show_beacons: showBeacons, show_sattrack: showSatTrack, show_grid: showGrid, grid_type: gridType,
        center_map_on_de: centerMapOnDe,
        prop_overlay: propOverlay, wx_overlay: wxOverlay, night_lights: nl, use_metric: mu,
        display_power_method: dpm, font_path: fontPath,
        prop_band: document.getElementById('prop-band').value,
        prop_mode: document.getElementById('prop-mode').value,
        prop_power: document.getElementById('prop-power').value,
        prop_toa: document.getElementById('prop-toa').value,
        prop_path: (document.querySelector('input[name="prop-path"]:checked') || {value:'0'}).value,
        prop_ant_gain: document.getElementById('prop-ant-gain').value,
        muf_opacity: document.getElementById('muf-opacity').value
      });
      try {
        const r = await fetch('/set_config?' + params);
        const t = await r.text();
        const el = document.getElementById('app-msg');
        el.textContent = 'Saved!';
        el.className = '';
        setTimeout(() => el.textContent = '', 3000);
      } catch(e) {}
    }

    async function setPower(state) {
      try {
        const r = await fetch('/api/display/power', {
          method: 'POST',
          body: JSON.stringify({state}),
          headers: {'Content-Type': 'application/json'}
        });
        const j = await r.json();
        document.getElementById('pwr-msg').textContent = 'State: ' + j.state;
      } catch(e) {}
    }

    async function saveConfig() {
      const call = document.getElementById('call').value.trim();
      const callFrn = document.getElementById('call-frn').value.trim();
      const grid = document.getElementById('grid').value.trim();
      const lat  = document.getElementById('lat').value;
      const lon  = document.getElementById('lon').value;
      const gpsEnabled = document.getElementById('gps-enabled').checked ? '1' : '0';
      const audioMuted = document.getElementById('audio-muted').checked ? '1' : '0';
      const audioVolume = document.getElementById('audio-volume').value;
      const callFg = document.getElementById('call-fg').value;
      const callBg = document.getElementById('call-bg').value;
      const params = new URLSearchParams({
        call, callsign_frn: callFrn, grid, lat, lon, 
        gps_enabled: gpsEnabled, audio_muted: audioMuted,
        audio_volume: audioVolume,
        callsign_color: callFg, callsign_bg_color: callBg
      });

      const tzPreset = document.getElementById('default-tz-preset').value;
      if (tzPreset.startsWith('custom')) {
        params.set('default_tz_offset', document.getElementById('default-tz-offset').value);
        params.set('default_tz_label', document.getElementById('default-tz-label').value || 'UTC');
      } else {
        const parts = tzPreset.split('|');
        params.set('default_tz_offset', parts[0]);
        params.set('default_tz_label', parts[1]);
      }

      try {
        const r = await fetch('/set_config?' + params);
        const t = await r.text();
        setMsg(t === 'ok' ? 'Saved!' : 'Error: ' + t, t !== 'ok');
      } catch(e) { setMsg('Save failed: ' + e, true); }
    }

    function setMsg(text, isErr) {
      const el = document.getElementById('msg');
      el.textContent = text;
      el.className = isErr ? 'err' : '';
      if (!isErr) setTimeout(() => el.textContent = '', 3000);
    }

    async function refreshStatus() {
      // UTC time
      try {
        const r = await fetch('/get_time.txt');
        const kv = parseKV(await r.text());
        document.getElementById('utc-time').textContent = kv['Clock_UTC'] || '—';
      } catch(e) {}

      // Performance
      try {
        const r = await fetch('/debug/performance');
        const j = await r.json();
        document.getElementById('fps').textContent = j.fps ? j.fps.toFixed(1) : '—';
        const sec = j.running_since || 0;
        const h = Math.floor(sec/3600), m = Math.floor((sec%3600)/60), s = sec%60;
        document.getElementById('uptime').textContent =
          `${h}h ${m}m ${s}s`;
      } catch(e) {}

      // Services
      try {
        const r = await fetch('/debug/health');
        const j = await r.json();
        let html = '';
        for (const [name, st] of Object.entries(j)) {
          const cls = st.ok ? 'ok' : 'err';
          const err = st.ok ? (st.lastSuccess || '—') : (st.lastError || 'error');
          html += `<div class="status-row"><span>${name}</span><span class="${cls}">${st.ok ? '✓' : '✗'} <span class="dim">${err}</span></span></div>`;
        }
        document.getElementById('services-card').innerHTML = html || '<span class="dim">No services</span>';
      } catch(e) {}
    }

    async function refreshDeDx() {
      try {
        const r = await fetch('/get_de.txt');
        const kv = parseKV(await r.text());
        document.getElementById('de-info').innerHTML =
          `<b>${kv['DE_Callsign']||'—'}</b> &nbsp; ${kv['DE_Grid']||''}<br>
           ${kv['DE_Lat']||''}, ${kv['DE_Lon']||''}`;
      } catch(e) {}
      try {
        const r = await fetch('/get_dx.txt');
        const text = await r.text();
        if (text.startsWith('DX not set')) {
          document.getElementById('dx-info').textContent = 'Not set';
        } else {
          const kv = parseKV(text);
          document.getElementById('dx-info').innerHTML =
            `Grid: <b>${kv['DX_Grid']||'—'}</b><br>
             ${kv['DX_Lat']||''}, ${kv['DX_Lon']||''}<br>
             Dist: ${kv['DX_Dist_km']||'—'} km &nbsp; Bearing: ${kv['DX_Bearing']||'—'}°`;
        }
      } catch(e) {}
    }

    // Poll UTC time every 5 s when status tab is visible
    setInterval(() => {
      if (document.getElementById('status').classList.contains('active'))
        refreshStatus();
    }, 5000);

    async function loadNetwork() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('cors-proxy-url').value = c.corsProxyUrl || '';
        document.getElementById('hub-mode').value = c.hubMode || 'Off';
        document.getElementById('hub-ip').value = c.hubIp || '';
        document.getElementById('hub-port').value = c.hubPort || 8080;
        toggleHubFields();
      } catch(e) {}
    }

    function toggleHubFields() {
      const mode = document.getElementById('hub-mode').value;
      document.getElementById('hub-client-fields').style.display = (mode === 'Client') ? 'block' : 'none';
    }

    async function saveNetwork() {
      const url = document.getElementById('cors-proxy-url').value.trim();
      const mode = document.getElementById('hub-mode').value;
      const ip = document.getElementById('hub-ip').value.trim();
      const port = document.getElementById('hub-port').value;
      const params = new URLSearchParams({
        cors_proxy_url: url,
        hub_mode: mode,
        hub_ip: ip,
        hub_port: port
      });
      try {
        const r = await fetch('/set_config?' + params);
        const t = await r.text();
        const el = document.getElementById('net-msg');
        el.textContent = t === 'ok' ? 'Saved! Reload app to apply.' : 'Error: ' + t;
        el.className = t !== 'ok' ? 'err' : '';
        if (t === 'ok') setTimeout(() => el.textContent = '', 3000);
      } catch(e) {}
    }

    function showTabMsg(id, ok) {
      const el = document.getElementById(id);
      el.textContent = ok ? 'Saved!' : 'Error saving';
      el.className = ok ? '' : 'err';
      if (ok) setTimeout(() => el.textContent = '', 3000);
    }

    async function loadCluster() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('dx-enabled').checked = !!c.dxClusterEnabled;
        document.getElementById('rbn-enabled').checked = !!c.rbnEnabled;
        document.getElementById('dx-hide-dup').checked = !!c.dxClusterHideDuplicates;
        document.getElementById('dx-max-age').value = c.dxClusterMaxAgeMinutes || 20;
        document.getElementById('dx-host').value = c.dxClusterHost || '';
        document.getElementById('dx-port').value = c.dxClusterPort || 7300;
        document.getElementById('dx-login').value = c.dxClusterLogin || '';
        document.getElementById('dx-wsjtx').checked = !!c.dxClusterUseWSJTX;
        document.getElementById('wsjtx-port').value = c.wsjtxPort || 2237;
      } catch(e) {}
    }

    async function saveCluster() {
      const params = new URLSearchParams({
        dx_enabled: document.getElementById('dx-enabled').checked ? '1' : '0',
        rbn_enabled: document.getElementById('rbn-enabled').checked ? '1' : '0',
        dx_hide_duplicates: document.getElementById('dx-hide-dup').checked ? '1' : '0',
        dx_max_age: document.getElementById('dx-max-age').value,
        dx_host: document.getElementById('dx-host').value.trim(),
        dx_port: document.getElementById('dx-port').value,
        dx_login: document.getElementById('dx-login').value.trim(),
        dx_use_wsjtx: document.getElementById('dx-wsjtx').checked ? '1' : '0',
        wsjtx_port: document.getElementById('wsjtx-port').value
      });
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('cluster-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('cluster-msg', false); }
    }

    async function loadRadio() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('rig-host').value = c.rigHost || '';
        document.getElementById('rig-port').value = c.rigPort || 4532;
        document.getElementById('rig-autotune').checked = !!c.rigAutoTune;
        document.getElementById('rot-host').value = c.rotatorHost || '';
        document.getElementById('rot-port').value = c.rotatorPort || 4533;
        document.getElementById('rot-autotrack').checked = !!c.rotatorAutoTrack;
        document.getElementById('rot-upover').checked = !!c.rotatorUpover;
      } catch(e) {}
    }

    async function saveRadio() {
      const params = new URLSearchParams({
        rig_host: document.getElementById('rig-host').value.trim(),
        rig_port: document.getElementById('rig-port').value,
        rig_auto_tune: document.getElementById('rig-autotune').checked ? '1' : '0',
        rot_host: document.getElementById('rot-host').value.trim(),
        rot_port: document.getElementById('rot-port').value,
        rot_auto_track: document.getElementById('rot-autotrack').checked ? '1' : '0',
        rot_upover: document.getElementById('rot-upover').checked ? '1' : '0'
      });
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('radio-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('radio-msg', false); }
    }

    async function loadServices() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('qrz-user').value = c.qrzUsername || '';
        document.getElementById('lotw-call').value = c.lotwCall || '';
        document.getElementById('clublog-key').value = c.clublogApiKey || '';
        document.getElementById('rb-key').value = c.repeaterBookKey || '';
        document.getElementById('wl-key').value = c.winlinkKey || '';
        document.getElementById('spot-source').value = c.liveSpotSource || 'PSK';
        document.getElementById('spot-age').value = c.liveSpotsMaxAge || 30;
        const bitmask = (c.liveSpotsBands !== undefined) ? c.liveSpotsBands : 0xFFF;
        document.querySelectorAll('#band-chips .chip').forEach((ch, i) => {
          ch.classList.toggle('active', !!(bitmask & (1 << i)));
        });
        document.getElementById('gps-enabled-serv').checked = !!c.gpsEnabled;
        document.getElementById('rss-enabled').checked = !!c.rssEnabled;
        document.getElementById('onta-filter').value = c.ontaFilter || 'all';
        document.getElementById('onta-max-dist').value = c.ontaMaxDistKm || 0;
        document.getElementById('k-index-threshold').value = c.kIndexAlertThreshold || 4.0;
        document.getElementById('spots-of-de').checked = !!c.liveSpotsOfDe;
        document.getElementById('spots-use-call').checked = !!c.liveSpotsUseCall;
      } catch(e) {}
    }

    async function saveServices() {
      let bitmask = 0;
      document.querySelectorAll('#band-chips .chip').forEach((ch, i) => {
        if (ch.classList.contains('active')) bitmask |= (1 << i);
      });
      const params = new URLSearchParams({
        qrz_user: document.getElementById('qrz-user').value.trim(),
        lotw_call: document.getElementById('lotw-call').value.trim(),
        clublog_api_key: document.getElementById('clublog-key').value.trim(),
        rb_key: document.getElementById('rb-key').value.trim(),
        wl_key: document.getElementById('wl-key').value.trim(),
        spot_source: document.getElementById('spot-source').value,
        spot_max_age: document.getElementById('spot-age').value,
        spot_bands: bitmask,
        gps_enabled: document.getElementById('gps-enabled-serv').checked ? '1' : '0',
        rss_enabled: document.getElementById('rss-enabled').checked ? '1' : '0',
        onta_filter: document.getElementById('onta-filter').value,
        onta_max_dist: document.getElementById('onta-max-dist').value,
        k_index_threshold: document.getElementById('k-index-threshold').value,
        live_spots_of_de: document.getElementById('spots-of-de').checked ? '1' : '0',
        live_spots_use_call: document.getElementById('spots-use-call').checked ? '1' : '0'
      });
      const qrzPass = document.getElementById('qrz-pass').value;
      if (qrzPass) params.set('qrz_pass', qrzPass);
      const lotwPass = document.getElementById('lotw-pass').value;
      if (lotwPass) params.set('lotw_pass', lotwPass);
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('services-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('services-msg', false); }
    }

    async function loadBrightness() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        const lvl = c.brightness || 100;
        document.getElementById('bright-level').value = lvl;
        document.getElementById('bright-pct').textContent = lvl;
        document.getElementById('bright-schedule').checked = !!c.brightnessSchedule;
        const dh = String(c.dimHour !== undefined ? c.dimHour : 22).padStart(2,'0');
        const dm = String(c.dimMinute !== undefined ? c.dimMinute : 0).padStart(2,'0');
        document.getElementById('dim-time').value = dh + ':' + dm;
        const bh = String(c.brightHour !== undefined ? c.brightHour : 6).padStart(2,'0');
        const bm = String(c.brightMinute !== undefined ? c.brightMinute : 0).padStart(2,'0');
        document.getElementById('bright-time').value = bh + ':' + bm;
      } catch(e) {}
    }

    async function saveBrightness() {
      const dimParts = (document.getElementById('dim-time').value || '22:00').split(':');
      const brightParts = (document.getElementById('bright-time').value || '06:00').split(':');
      const params = new URLSearchParams({
        brightness: document.getElementById('bright-level').value,
        brightness_schedule: document.getElementById('bright-schedule').checked ? '1' : '0',
        dim_hour: dimParts[0] || '22',
        dim_min: dimParts[1] || '0',
        bright_hour: brightParts[0] || '6',
        bright_min: brightParts[1] || '0'
      });
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('bright-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('bright-msg', false); }
    }

    let availableWidgets = [];
    let availableSatellites = [];
    async function loadWidgets() {
      try {
        const r1 = await fetch('/api/widgets/available');
        availableWidgets = await r1.json();
        availableWidgets.sort((a, b) => a.display.localeCompare(b.display));
        const r2 = await fetch('/api/config');
        const c = await r2.json();
        const r3 = await fetch('/api/satellites');
        availableSatellites = await r3.json();

        // Pane 4 (index 3) is a small pane restricted to fixed set of widgets
        const pane4Allowed = new Set(['NCDXF', 'Solar', 'DX Weather', 'DE Weather', 'Band Cond']);

        // Populate satellites
        const satSel = document.getElementById('selected-satellite');
        const satWidSel = document.getElementById('sat-widget-satellite');
        satSel.innerHTML = '';
        satWidSel.innerHTML = '';
        availableSatellites.forEach(s => {
          const opt = document.createElement('option');
          opt.value = opt.textContent = s;
          satSel.appendChild(opt);
          satWidSel.appendChild(opt.cloneNode(true));
        });

        document.getElementById('rot-interval').value = c.rotationInterval || 30;
        document.getElementById('sync-rot').checked = !!c.syncRotation;

        // Populate Panels 1-6
        for (let i = 0; i < 6; i++) {
          const list = document.getElementById(`pane${i}-list`);
          list.innerHTML = '';
          const activeRot = c.panes ? (c.panes[i] ? c.panes[i].rotation : []) : [];

          let widgets = availableWidgets;
          if (i === 3) widgets = availableWidgets.filter(w => pane4Allowed.has(w.display));

          widgets.forEach(w => {
            const div = document.createElement('div');
            div.className = 'widget-item' + (activeRot.includes(w.display) ? ' active' : '');
            div.textContent = w.display;
            div.dataset.id = w.id;
            div.dataset.scrollable = w.isScrollable ? '1' : '0';
            div.onclick = () => {
              if (div.classList.contains('disabled')) return;
              div.classList.toggle('active');
            };
            list.appendChild(div);
          });
        }

        // Full Height state
        const p6Rot = (c.panes && c.panes[5]) ? c.panes[5].rotation : [];
        document.getElementById('full-height').checked = (p6Rot.length === 0);
        toggleFullHeight();

        document.getElementById('panel-mode').value = c.panelMode || 'dx';
        document.getElementById('selected-satellite').value = c.selectedSatellite || '';
        document.getElementById('sat-widget-satellite').value = c.sat_widget_satellite || '';
        toggleSatSelect();
      } catch(e) {}
    }

    async function loadWidgetConfig() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();

        // Alarms
        const ah = String(c.alarmTimeHH !== undefined ? c.alarmTimeHH : 0).padStart(2, '0');
        const am = String(c.alarmTimeMM !== undefined ? c.alarmTimeMM : 0).padStart(2, '0');
        document.getElementById('alarm-time').value = ah + ':' + am;
        document.getElementById('alarm-armed').checked = !!c.alarmArmed;
        document.getElementById('alarm-utc').value = c.alarmUtc ? '1' : '0';

        // SDO
        const sdoWl = c.sdoWavelength || '0193';
        document.getElementById('sdo-wavelength').value = sdoWl;
        document.getElementById('sdo-rotating').checked = !!c.sdoRotating;
        document.getElementById('sdo-pfss').checked = !!c.sdoPfss;

        // Marine
        document.getElementById('marine-station').value = c.marineStation || '';
        document.getElementById('marine-station-name').textContent = c.marineStationName || '';
        document.getElementById('marine-buoy').value = c.marineBuoy || '';

        // Big Clock
        document.getElementById('bc-digital').value = c.bigClockDigital ? '1' : '0';
        document.getElementById('bc-12h').value = c.bigClock12h ? '1' : '0';
        document.getElementById('bc-utc').checked = !!c.bigClockUtc;
        document.getElementById('bc-sec').checked = !!c.bigClockShowSec;
        document.getElementById('bc-date').checked = !!c.bigClockShowDate;
        document.getElementById('bc-hue').value = c.bigClockHue !== undefined ? c.bigClockHue : 0;

        // Aux Clock
        const tzOff = c.auxClockTzOffset !== undefined ? c.auxClockTzOffset : 0;
        const tzLbl = c.auxClockTzLabel || 'UTC';
        const tzPresets = [{v:999,l:'Local'},{v:0,l:'UTC'},{v:-5,l:'EST'},
                           {v:-6,l:'CST'},{v:-7,l:'MST'},{v:-8,l:'PST'},
                           {v:1,l:'CET'},{v:9,l:'JST'},{v:10,l:'AEST'}];
        const match = tzPresets.find(p => p.v === tzOff && p.l === tzLbl);
        document.getElementById('aux-tz-preset').value = match ? (tzOff+'|'+tzLbl) : 'custom|custom';
        if (!match) {
          document.getElementById('aux-tz-offset').value = tzOff;
          document.getElementById('aux-tz-label').value  = tzLbl;
        }
        document.getElementById('aux-star-mode').value = c.auxClockStarMode !== undefined ? c.auxClockStarMode : 1;
        toggleAuxCustom();

        // Misc
        document.getElementById('idle-minutes').value = c.idleMinutes !== undefined ? c.idleMinutes : 0;
        document.getElementById('ltr329-auto-dim').checked = !!c.ltr329AutoDim;
        document.getElementById('prevent-sleep').checked = !!c.preventSleep;

        // Countdown
        document.getElementById('countdown-label').value = c.countdownLabel || '';
        document.getElementById('countdown-time').value = c.countdownTime || '';

      } catch(e) {}
    }

    function toggleFullHeight() {
      const isFull = document.getElementById('full-height').checked;
      const pane4List = document.getElementById('pane4-list');
      const pane5List = document.getElementById('pane5-list');
      const p5Container = document.getElementById('pane5-container');

      p5Container.style.opacity = isFull ? '0.3' : '1';
      p5Container.style.pointerEvents = isFull ? 'none' : 'auto';

      pane4List.querySelectorAll('.widget-item').forEach(el => {
        const scrollable = el.dataset.scrollable === '1';
        el.classList.toggle('disabled', isFull && !scrollable);
        if (isFull && !scrollable) el.classList.remove('active');
      });

      if (isFull) {
        pane5List.querySelectorAll('.widget-item').forEach(el => el.classList.remove('active'));
      }
    }

    function toggleSatSelect() {
      const pm = document.getElementById('panel-mode').value;
      document.getElementById('sat-select-container').style.display = (pm === 'sat') ? 'block' : 'none';
    }

    function toggleAuxCustom() {
      const v = document.getElementById('aux-tz-preset').value;
      document.getElementById('aux-custom-fields').style.display = v.startsWith('custom') ? 'block' : 'none';
    }

    async function saveWidgets() {
      const params = new URLSearchParams({
        rotation_interval: document.getElementById('rot-interval').value,
        sync_rotation: document.getElementById('sync-rot').checked ? '1' : '0',
        panel_mode: document.getElementById('panel-mode').value,
        selected_satellite: document.getElementById('selected-satellite').value,
        sat_widget_satellite: document.getElementById('sat-widget-satellite').value
      });

      for (let i = 0; i < 6; i++) {
        const active = [];
        document.querySelectorAll(`#pane${i}-list .widget-item.active`).forEach(el => {
          active.push(el.dataset.id);
        });
        params.set(`pane${i}`, active.join(','));
      }
 
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('widgets-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('widgets-msg', false); }
    }

    async function saveWidgetConfig() {
      const alarmParts = (document.getElementById('alarm-time').value || '00:00').split(':');
      const params = new URLSearchParams({
        alarm_armed: document.getElementById('alarm-armed').checked ? '1' : '0',
        alarm_hour: alarmParts[0] || '0',
        alarm_min: alarmParts[1] || '0',
        alarm_utc: document.getElementById('alarm-utc').value,
        sdo_wavelength: document.getElementById('sdo-wavelength').value.trim(),
        sdo_rotating: document.getElementById('sdo-rotating').checked ? '1' : '0',
        sdo_pfss: document.getElementById('sdo-pfss').checked ? '1' : '0',
        marine_station: document.getElementById('marine-station').value.trim(),
        marine_buoy: document.getElementById('marine-buoy').value.trim(),
        bc_digital: document.getElementById('bc-digital').value,
        bc_12h: document.getElementById('bc-12h').value,
        bc_utc: document.getElementById('bc-utc').checked ? '1' : '0',
        bc_sec: document.getElementById('bc-sec').checked ? '1' : '0',
        bc_date: document.getElementById('bc-date').checked ? '1' : '0',
        bc_hue: document.getElementById('bc-hue').value,
        idle_minutes: document.getElementById('idle-minutes').value,
        ltr329_auto_dim: document.getElementById('ltr329-auto-dim').checked ? '1' : '0',
        prevent_sleep: document.getElementById('prevent-sleep').checked ? '1' : '0',
        aux_star_mode: document.getElementById('aux-star-mode').value,
        countdown_label: document.getElementById('countdown-label').value.trim(),
        countdown_time: document.getElementById('countdown-time').value.trim()
      });

      const auxPreset = document.getElementById('aux-tz-preset').value;
      if (auxPreset.startsWith('custom')) {
        params.set('aux_tz_offset', document.getElementById('aux-tz-offset').value);
        params.set('aux_tz_label', document.getElementById('aux-tz-label').value || 'UTC');
      } else {
        const parts = auxPreset.split('|');
        params.set('aux_tz_offset', parts[0]);
        params.set('aux_tz_label', parts[1]);
      }

      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('wcfg-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('wcfg-msg', false); }
    }

    async function loadWatchlist() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('watchlist-text').value = (c.watchlist || []).join('\n');
      } catch(e) {}
    }

    async function saveWatchlist() {
      const text = document.getElementById('watchlist-text').value;
      const params = new URLSearchParams({ watchlist: text });
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('watchlist-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('watchlist-msg', false); }
    }

    async function loadUpdate() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('sys-version').textContent = c.version || '—';
        document.getElementById('sys-arch').textContent = c.arch || '—';
        document.getElementById('sys-install').textContent = c.installType || '—';
        
        let instr = '', cmd = '';
        const type = c.installType;
        if (type === 'RPM') {
          instr = 'To update via DNF, run:';
          cmd = 'sudo dnf update hamclock-next';
        } else if (type === 'DEB') {
          instr = 'To update, download the latest .deb and install it:';
          cmd = 'sudo apt install ./hamclock-next.deb';
        } else if (type === 'WASM') {
          instr = 'To update HamClock-Next in the browser:';
          cmd = 'Please reload the page to update.';
        } else {
          instr = 'To update the binary installation:';
          cmd = 'Download the latest release from GitHub.';
        }
        document.getElementById('update-instr').textContent = instr;
        document.getElementById('update-cmd').textContent = cmd;
      } catch(e) {}
    }

    // Init
    // Build band chips (160m→2m, bit 0=160m, bit 11=2m)
    (function() {
      const bands = ['160m','80m','60m','40m','30m','20m','17m','15m','12m','10m','6m','2m'];
      const container = document.getElementById('band-chips');
      bands.forEach((b, i) => {
        const ch = document.createElement('span');
        ch.className = 'chip active';
        ch.textContent = b;
        ch.onclick = () => ch.classList.toggle('active');
        container.appendChild(ch);
      });
    })();
    loadConfig();
    loadNetwork();
  </script>
</body>
</html>)HTML";
    res.set_content(html, "text/html");
  });

  registerRoutes(svr);
  svr.listen("0.0.0.0", port_);
#endif
}
