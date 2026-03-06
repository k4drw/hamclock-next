#include "WebServer.h"
#include "../ui/PaneContainer.h"
#include "NetworkManager.h"

#include <SDL.h>

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/StringUtils.h"
#include "../core/WatchlistStore.h"
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

static SDL_Color hexToColor(const std::string &hex) {
  SDL_Color c = {255, 255, 255, 255};
  if (hex.empty())
    return c;
  std::string s = hex;
  if (s[0] == '#')
    s = s.substr(1);
  if (s.length() == 6) {
    unsigned int r, g, b;
    if (std::sscanf(s.c_str(), "%02x%02x%02x", &r, &g, &b) == 3) {
      c.r = (uint8_t)r;
      c.g = (uint8_t)g;
      c.b = (uint8_t)b;
    }
  }
  return c;
}

[[maybe_unused]] static std::string propOverlayToString(PropOverlayType t) {
  switch (t) {
  case PropOverlayType::None:
    return "none";
  case PropOverlayType::Muf:
    return "muf";
  case PropOverlayType::Voacap:
    return "voacap";
  case PropOverlayType::Reliability:
    return "reliability";
  case PropOverlayType::Toa:
    return "toa";
  case PropOverlayType::Heatmap:
    return "heatmap";
  case PropOverlayType::Drap:
    return "drap";
  case PropOverlayType::Aurora:
    return "aurora";
  }
  return "none";
}

[[maybe_unused]] static PropOverlayType
propOverlayFromString(const std::string &s) {
  if (s == "muf")
    return PropOverlayType::Muf;
  if (s == "voacap")
    return PropOverlayType::Voacap;
  if (s == "reliability")
    return PropOverlayType::Reliability;
  if (s == "toa")
    return PropOverlayType::Toa;
  if (s == "heatmap")
    return PropOverlayType::Heatmap;
  if (s == "drap")
    return PropOverlayType::Drap;
  if (s == "aurora")
    return PropOverlayType::Aurora;
  return PropOverlayType::None;
}

[[maybe_unused]] static std::string wxOverlayToString(WeatherOverlayType t) {
  switch (t) {
  case WeatherOverlayType::None:
    return "none";
  case WeatherOverlayType::Clouds:
    return "clouds";
  case WeatherOverlayType::WxMb:
    return "wxmb";
  }
  return "none";
}

[[maybe_unused]] static WeatherOverlayType
wxOverlayFromString(const std::string &s) {
  if (s == "clouds")
    return WeatherOverlayType::Clouds;
  if (s == "wxmb")
    return WeatherOverlayType::WxMb;
  return WeatherOverlayType::None;
}

static std::string base64Decode(const std::string &in) {
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++)
    T[(unsigned char)chars[i]] = i;
  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (T[c] == -1)
      break;
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
}

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

static bool isPrivateOrLoopbackUrl(const std::string &url) {
  size_t pos = url.find("://");
  if (pos == std::string::npos)
    return true;
  pos += 3;
  size_t end = url.find_first_of("/:?#", pos);
  std::string host =
      (end == std::string::npos) ? url.substr(pos) : url.substr(pos, end - pos);
  if (!host.empty() && host.front() == '[')
    host = host.substr(1, host.size() >= 2 ? host.size() - 2 : 0);
  if (host == "localhost" || host == "::1")
    return true;
  unsigned int a, b, c, d;
  if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
    if (a == 127 || a == 10 || (a == 192 && b == 168) ||
        (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254))
      return true;
  }
  return false;
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
    : renderer_(renderer), cfg_(&cfg), state_(&state), cfgMgr_(&cfgMgr),
      watchlist_(watchlist), solar_(solar), contests_(contests), dxc_(dxc),
      spots_(spots), cpu_(cpu), displayPower_(displayPower),
      reloadFlag_(&reloadFlag), port_(port) {}

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
    <div class="tab" onclick="showTab('watchlist')">Watchlist</div>
    <div class="tab" onclick="showTab('update')">Update</div>
  </div>

  <div id="identity" class="panel active">
    <div class="card">
      <label>Callsign</label>
      <input type="text" id="call" maxlength="12">
      <label>Grid Square</label>
      <input type="text" id="grid" maxlength="8">
      <label>Latitude</label>
      <input type="number" id="lat" step="0.0001" min="-90" max="90">
      <label>Longitude</label>
      <input type="number" id="lon" step="0.0001" min="-180" max="180">
      <button onclick="saveConfig()">Save</button>
      <div id="msg"></div>
    </div>
  </div>

  <div id="appearance" class="panel">
    <div class="card">
      <label>Color Theme</label>
      <select id="theme">
        <option value="default">Default (Orange)</option>
        <option value="dark">Modern Dark</option>
        <option value="glass">Glass</option>
      </select>
      <label>Map Style</label>
      <select id="map-style">
        <option value="nasa">NASA Blue Marble</option>
        <option value="terrain">Terrain</option>
        <option value="countries">Countries</option>
      </select>
      <label>Projection</label>
      <select id="projection">
        <option value="equirectangular">Equirectangular</option>
        <option value="robinson">Robinson</option>
      </select>
      <label style="margin-top:4px"><input type="checkbox" id="show-borders"> Show Country Borders</label>
      <label>Propagation Overlay</label>
      <select id="prop-overlay">
        <option value="none">None</option>
        <option value="muf">MUF</option>
        <option value="voacap">VOACAP</option>
        <option value="reliability">Reliability</option>
        <option value="toa">Time of Arrival</option>
        <option value="heatmap">Heatmap</option>
        <option value="drap">DRAP</option>
        <option value="aurora">Aurora</option>
      </select>
      <label>Weather Overlay</label>
      <select id="weather-overlay">
        <option value="none">None</option>
        <option value="clouds">Clouds</option>
        <option value="wxmb">WxMB</option>
      </select>
      <label style="margin-top:10px"><input type="checkbox" id="night-lights"> Show Night Lights</label>
      <label><input type="checkbox" id="use-metric"> Use Metric Units</label>
      <button onclick="saveAppearance()" style="margin-top:10px">Save Appearance</button>
      <div id="app-msg"></div>
    </div>
    <div class="card">
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
    <div class="card">
      <label>CORS Proxy URL</label>
      <input type="text" id="cors-proxy-url" placeholder="/proxy/">
      <div class="dim" style="margin-bottom:10px">
        Prefix prepended to external API URLs in WASM builds.<br>
        Default <code>/proxy/</code> uses the bundled serve.py proxy.<br>
        Leave empty only if your server already sends CORS headers.
      </div>
      <div class="section-hdr" style="margin-top:16px">Local Data Hub</div>
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
      <input type="password" id="qrz-pass" placeholder="(unchanged if blank)">
      <label>RepeaterBook Key</label>
      <input type="text" id="rb-key">
      <label>Winlink Key</label>
      <input type="text" id="wl-key">
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
      <label><input type="checkbox" id="gps-enabled"> GPS Auto-Location</label>
      <label><input type="checkbox" id="rss-enabled"> RSS News Banner</label>
      <label>POTA/SOTA Filter</label>
      <select id="onta-filter">
        <option value="all">All Spots</option>
        <option value="pota">POTA Only</option>
        <option value="sota">SOTA Only</option>
      </select>
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
      <div style="display:grid; grid-template-columns: 1fr 1fr; gap:10px">
        <div>
          <label>Pane 5 (Top Right)</label>
          <div id="pane4-list" class="widget-list"></div>
        </div>
        <div>
          <label>Pane 6 (Bottom Right)</label>
          <div id="pane5-list" class="widget-list"></div>
        </div>
      </div>
      
      <button onclick="saveWidgets()" style="margin-top:16px">Save Widgets</button>
      <div id="widgets-msg"></div>
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
  </style>

  <script>
    // Tab navigation
    function showTab(name) {
      const ids = ['identity','appearance','status','de-dx','network','cluster','radio','services','brightness','widgets','watchlist','update'];
      document.querySelectorAll('.tab').forEach((t,i) => t.classList.toggle('active', ids[i] === name));
      document.querySelectorAll('.panel').forEach(p => p.classList.toggle('active', p.id === name));
      if (name === 'appearance') loadAppearance();
      if (name === 'status') refreshStatus();
      if (name === 'de-dx') refreshDeDx();
      if (name === 'network') loadNetwork();
      if (name === 'cluster') loadCluster();
      if (name === 'radio') loadRadio();
      if (name === 'services') loadServices();
      if (name === 'brightness') loadBrightness();
      if (name === 'widgets') loadWidgets();
      if (name === 'watchlist') loadWatchlist();
      if (name === 'update') loadUpdate();
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
        document.getElementById('grid').value = c.grid || '';
        document.getElementById('lat').value = c.lat || '';
        document.getElementById('lon').value = c.lon || '';
      } catch(e) { setMsg('Failed to load config: ' + e, true); }
    }

    async function loadAppearance() {
      try {
        const r = await fetch('/api/config');
        const c = await r.json();
        document.getElementById('theme').value = c.theme || 'default';
        document.getElementById('map-style').value = c.mapStyle || 'nasa';
        document.getElementById('projection').value = c.projection || 'equirectangular';
        document.getElementById('show-borders').checked = !!c.showBorders;
        document.getElementById('prop-overlay').value = c.propOverlay || 'none';
        document.getElementById('weather-overlay').value = c.weatherOverlay || 'none';
        document.getElementById('night-lights').checked = !!c.mapNightLights;
        document.getElementById('use-metric').checked = !!c.useMetric;
        const r2 = await fetch('/api/display/status');
        const j = await r2.json();
        document.getElementById('pwr-msg').textContent = 'State: ' + j.power + ' (' + j.method + ')';
      } catch(e) {}
    }

    async function saveAppearance() {
      const theme = document.getElementById('theme').value;
      const mapStyle = document.getElementById('map-style').value;
      const projection = document.getElementById('projection').value;
      const showBorders = document.getElementById('show-borders').checked ? '1' : '0';
      const propOverlay = document.getElementById('prop-overlay').value;
      const wxOverlay = document.getElementById('weather-overlay').value;
      const nl = document.getElementById('night-lights').checked ? '1' : '0';
      const mu = document.getElementById('use-metric').checked ? '1' : '0';
      const params = new URLSearchParams({
        theme, map_style: mapStyle, projection, show_borders: showBorders,
        prop_overlay: propOverlay, wx_overlay: wxOverlay, night_lights: nl, use_metric: mu
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
      const grid = document.getElementById('grid').value.trim();
      const lat  = document.getElementById('lat').value;
      const lon  = document.getElementById('lon').value;
      const params = new URLSearchParams({call, grid, lat, lon});
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
        document.getElementById('rb-key').value = c.repeaterBookKey || '';
        document.getElementById('wl-key').value = c.winlinkKey || '';
        document.getElementById('spot-source').value = c.liveSpotSource || 'PSK';
        document.getElementById('spot-age').value = c.liveSpotsMaxAge || 30;
        const bitmask = (c.liveSpotsBands !== undefined) ? c.liveSpotsBands : 0xFFF;
        document.querySelectorAll('#band-chips .chip').forEach((ch, i) => {
          ch.classList.toggle('active', !!(bitmask & (1 << i)));
        });
        document.getElementById('gps-enabled').checked = !!c.gpsEnabled;
        document.getElementById('rss-enabled').checked = !!c.rssEnabled;
        document.getElementById('onta-filter').value = c.ontaFilter || 'all';
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
        rb_key: document.getElementById('rb-key').value.trim(),
        wl_key: document.getElementById('wl-key').value.trim(),
        spot_source: document.getElementById('spot-source').value,
        spot_max_age: document.getElementById('spot-age').value,
        spot_bands: bitmask,
        gps_enabled: document.getElementById('gps-enabled').checked ? '1' : '0',
        rss_enabled: document.getElementById('rss-enabled').checked ? '1' : '0',
        onta_filter: document.getElementById('onta-filter').value,
        live_spots_of_de: document.getElementById('spots-of-de').checked ? '1' : '0',
        live_spots_use_call: document.getElementById('spots-use-call').checked ? '1' : '0'
      });
      const pass = document.getElementById('qrz-pass').value;
      if (pass) params.set('qrz_pass', pass);
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
    async function loadWidgets() {
      try {
        const r1 = await fetch('/api/widgets/available');
        availableWidgets = await r1.json();
        const r2 = await fetch('/api/config');
        const c = await r2.json();
        
        document.getElementById('rot-interval').value = c.rotationInterval || 30;
        document.getElementById('sync-rot').checked = !!c.syncRotation;
        
        for (let i = 0; i < 6; i++) {
          const list = document.getElementById(`pane${i}-list`);
          list.innerHTML = '';
          const activeRot = c.panes ? (c.panes[i] ? c.panes[i].rotation : []) : [];
          availableWidgets.forEach(w => {
            const div = document.createElement('div');
            div.className = 'widget-item' + (activeRot.includes(w.display) ? ' active' : '');
            div.textContent = w.display;
            div.onclick = () => {
              div.classList.toggle('active');
            };
            div.dataset.id = w.id;
            list.appendChild(div);
          });
        }
      } catch(e) {}
    }

    async function saveWidgets() {
      const panes = [];
      for (let i = 0; i < 6; i++) {
        const active = [];
        document.querySelectorAll(`#pane${i}-list .widget-item.active`).forEach(el => {
          active.push(el.dataset.id);
        });
        panes.push(active.join(','));
      }
      const params = new URLSearchParams({
        rotation_interval: document.getElementById('rot-interval').value,
        sync_rotation: document.getElementById('sync-rot').checked ? '1' : '0',
        pane0: panes[0], pane1: panes[1], pane2: panes[2],
        pane3: panes[3], pane4: panes[4], pane5: panes[5]
      });
      try {
        const r = await fetch('/set_config?' + params);
        showTabMsg('widgets-msg', await r.text() === 'ok');
      } catch(e) { showTabMsg('widgets-msg', false); }
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
          instr = 'An update is available via DNF.';
          cmd = 'sudo dnf update hamclock-next';
        } else if (type === 'DEB') {
          instr = 'Download the latest .deb and install it.';
          cmd = 'sudo apt install ./hamclock-next.deb';
        } else if (type === 'WASM') {
          instr = 'A new version of HamClock-Next is available.';
          cmd = 'Please reload the page to update.';
        } else {
          instr = 'Binary update available.';
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

  svr.Get("/api/widgets/available",
          [](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            static const WidgetType all[] = {
                WidgetType::SOLAR,        WidgetType::DX_CLUSTER,
                WidgetType::LIVE_SPOTS,   WidgetType::BAND_CONDITIONS,
                WidgetType::CONTESTS,     WidgetType::ON_THE_AIR,
                WidgetType::GIMBAL,       WidgetType::MOON,
                WidgetType::CLOCK_AUX,    WidgetType::DX_PEDITIONS,
                WidgetType::DE_WEATHER,   WidgetType::DX_WEATHER,
                WidgetType::NCDXF,        WidgetType::SDO,
                WidgetType::HISTORY_FLUX, WidgetType::HISTORY_KP,
                WidgetType::HISTORY_SSN,  WidgetType::DRAP,
                WidgetType::AURORA,       WidgetType::AURORA_GRAPH,
                WidgetType::ADIF,         WidgetType::EME_TOOL,
                WidgetType::SYS_INFO,     WidgetType::ASTEROID,
                WidgetType::ALERTS,       WidgetType::FORECAST,
                WidgetType::HURRICANE,    WidgetType::MARINE,
                WidgetType::GREYLINE_DX,  WidgetType::METEOR,
                WidgetType::IONOSONDE,    WidgetType::SOLAR_STORM};
            for (auto t : all) {
              nlohmann::json w;
              w["id"] = widgetTypeToString(t);
              w["display"] = widgetTypeDisplayName(t);
              j.push_back(w);
            }
            res.set_content(j.dump(), "application/json");
          });

  svr.Get("/api/panes",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            if (panes_) {
              for (const auto &p : *panes_) {
                nlohmann::json pj;
                pj["current"] = widgetTypeDisplayName(p->getActiveType());
                pj["paused"] = p->isPaused();
                nlohmann::json rot = nlohmann::json::array();
                for (auto t : p->getRotation())
                  rot.push_back(widgetTypeDisplayName(t));
                pj["rotation"] = rot;
                j.push_back(pj);
              }
            }
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/api/panes/rotate",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("pane"));
            if (panes_ && idx >= 0 && idx < (int)panes_->size()) {
              (*panes_)[idx]->forceAdvance();
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/panes/rotate_all",
          [this](const httplib::Request &, httplib::Response &res) {
            if (panes_) {
              for (auto &p : *panes_)
                p->forceAdvance();
              res.set_content("ok", "text/plain");
            } else
              res.status = 503;
          });

  svr.Get("/api/panes/pause",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("pane"));
            if (panes_ && idx >= 0 && idx < (int)panes_->size()) {
              (*panes_)[idx]->setPaused(!(*panes_)[idx]->isPaused());
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/panes/pause_all",
          [this](const httplib::Request &req, httplib::Response &res) {
            bool p = req.get_param_value("paused") == "1";
            if (panes_) {
              for (auto &pane : *panes_)
                pane->setPaused(p);
              res.set_content("ok", "text/plain");
            } else
              res.status = 503;
          });

  svr.Get("/api/panes/toggle", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    int pIdx = StringUtils::safe_stoi(req.get_param_value("pane"));
    std::string wId = req.get_param_value("widget");
    if (panes_ && pIdx >= 0 && pIdx < (int)panes_->size()) {
      auto &pane = (*panes_)[pIdx];
      std::vector<WidgetType> rot = pane->getRotation();
      WidgetType target = widgetTypeFromString(wId, WidgetType::SOLAR);
      auto it = std::find(rot.begin(), rot.end(), target);
      if (it != rot.end())
        rot.erase(it);
      else
        rot.push_back(target);
      if (rot.empty())
        rot.push_back(WidgetType::SOLAR);
      pane->setRotation(rot, cfg_->rotationIntervalS, cfg_->syncRotation);
      if (pIdx == 0)
        cfg_->pane1Rotation = rot;
      else if (pIdx == 1)
        cfg_->pane2Rotation = rot;
      else if (pIdx == 2)
        cfg_->pane3Rotation = rot;
      else if (pIdx == 3)
        cfg_->pane4Rotation = rot;
      else if (pIdx == 4)
        cfg_->pane5Rotation = rot;
      else if (pIdx == 5)
        cfg_->pane6Rotation = rot;
      if (cfgMgr_)
        cfgMgr_->save(*cfg_);
      res.set_content("ok", "text/plain");
    } else
      res.status = 404;
  });

  svr.Get("/api/presets",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            if (cfg_) {
              for (const auto &p : cfg_->presets)
                j.push_back(p.name);
            }
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/api/presets/apply",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("index"));
            if (cfg_ && idx >= 0 && idx < (int)cfg_->presets.size()) {
              cfgMgr_->applyPreset(*cfg_, idx);
              cfgMgr_->save(*cfg_);
              if (reloadFlag_)
                reloadFlag_->store(true, std::memory_order_release);
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/presets/save",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("name")) {
              res.status = 400;
              return;
            }
            std::string name = req.get_param_value("name");
            if (cfg_) {
              cfgMgr_->savePreset(*cfg_, name);
              cfgMgr_->save(*cfg_);
              res.set_content("ok", "text/plain");
            } else
              res.status = 503;
          });

  svr.Get("/api/presets/delete",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("index"));
            if (cfg_ && idx >= 0 && idx < (int)cfg_->presets.size()) {
              cfgMgr_->deletePreset(*cfg_, idx);
              cfgMgr_->save(*cfg_);
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/config", [this](const httplib::Request &,
                                httplib::Response &res) {
    nlohmann::json j;
    j["callsign"] = cfg_->callsign;
    j["grid"] = cfg_->grid;
    j["theme"] = cfg_->theme;
    j["projection"] = cfg_->projection;
    j["mapStyle"] = cfg_->mapStyle;
    j["showGrid"] = cfg_->showGrid;
    j["gridType"] = cfg_->gridType;
    j["showBorders"] = cfg_->showBorders;
    j["mapNightLights"] = cfg_->mapNightLights;
    j["useMetric"] = cfg_->useMetric;
    j["propOverlay"] = propOverlayToString(cfg_->propOverlay);
    j["weatherOverlay"] = wxOverlayToString(cfg_->weatherOverlay);
    j["hubMode"] = (cfg_->hubMode == HubMode::Master)
                       ? "Master"
                       : (cfg_->hubMode == HubMode::Client ? "Client" : "Off");
    j["hubIp"] = cfg_->hubIp;
    j["hubPort"] = cfg_->hubPort;
    j["dxClusterEnabled"] = cfg_->dxClusterEnabled;
    j["dxClusterHost"] = cfg_->dxClusterHost;
    j["dxClusterPort"] = cfg_->dxClusterPort;
    j["dxClusterLogin"] = cfg_->dxClusterLogin;
    j["dxClusterUseWSJTX"] = cfg_->dxClusterUseWSJTX;
    j["wsjtxPort"] = cfg_->wsjtxPort;
    j["rbnEnabled"] = cfg_->rbnEnabled;
    j["rigHost"] = cfg_->rigHost;
    j["rigPort"] = cfg_->rigPort;
    j["rigAutoTune"] = cfg_->rigAutoTune;
    j["rotatorHost"] = cfg_->rotatorHost;
    j["rotatorPort"] = cfg_->rotatorPort;
    j["rotatorAutoTrack"] = cfg_->rotatorAutoTrack;
    j["rotatorUpover"] = cfg_->rotatorUpover;
    j["qrzUsername"] = cfg_->qrzUsername;
    j["repeaterBookKey"] = cfg_->repeaterBookKey;
    j["winlinkKey"] = cfg_->winlinkKey;
    j["watchlist"] = cfg_->watchlist;
    j["rotationInterval"] = cfg_->rotationIntervalS;
    j["syncRotation"] = cfg_->syncRotation;
    j["ontaFilter"] = cfg_->ontaFilter;
    j["liveSpotSource"] =
        (cfg_->liveSpotSource == LiveSpotSource::PSK)
            ? "PSK"
            : (cfg_->liveSpotSource == LiveSpotSource::RBN ? "RBN" : "WSPR");
    j["liveSpotsMaxAge"] = cfg_->liveSpotsMaxAge;
    j["liveSpotsBands"] = cfg_->liveSpotsBands;
    j["liveSpotsOfDe"] = cfg_->liveSpotsOfDe;
    j["liveSpotsUseCall"] = cfg_->liveSpotsUseCall;
    j["gpsEnabled"] = cfg_->gpsEnabled;
    j["rssEnabled"] = cfg_->rssEnabled;
    j["brightness"] = cfg_->brightness;
    j["brightnessSchedule"] = cfg_->brightnessSchedule;
    j["dimHour"] = cfg_->dimHour;
    j["dimMinute"] = cfg_->dimMinute;
    j["brightHour"] = cfg_->brightHour;
    j["brightMinute"] = cfg_->brightMinute;
    j["version"] = HAMCLOCK_VERSION;
    j["arch"] = HAMCLOCK_ARCH;
    j["installType"] = HAMCLOCK_INSTALL_TYPE;

    nlohmann::json panes = nlohmann::json::array();
    auto addPane = [&](const std::vector<WidgetType> &rot) {
      nlohmann::json pj;
      nlohmann::json r = nlohmann::json::array();
      for (auto t : rot)
        r.push_back(widgetTypeDisplayName(t));
      pj["rotation"] = r;
      panes.push_back(pj);
    };
    addPane(cfg_->pane1Rotation);
    addPane(cfg_->pane2Rotation);
    addPane(cfg_->pane3Rotation);
    addPane(cfg_->pane4Rotation);
    addPane(cfg_->pane5Rotation);
    addPane(cfg_->pane6Rotation);
    j["panes"] = panes;

    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/set_config", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (req.has_param("call"))
      cfg_->callsign = req.get_param_value("call");
    if (req.has_param("grid"))
      cfg_->grid = req.get_param_value("grid");
    if (req.has_param("projection"))
      cfg_->projection = req.get_param_value("projection");
    if (req.has_param("map_style")) {
      std::string s = req.get_param_value("map_style");
      if (!s.empty())
        cfg_->mapStyle = s;
    }
    if (req.has_param("show_grid"))
      cfg_->showGrid = req.get_param_value("show_grid") == "1";
    if (req.has_param("show_borders"))
      cfg_->showBorders = req.get_param_value("show_borders") == "1";
    if (req.has_param("night_lights"))
      cfg_->mapNightLights = req.get_param_value("night_lights") == "1";
    if (req.has_param("use_metric"))
      cfg_->useMetric = req.get_param_value("use_metric") == "1";
    if (req.has_param("grid_type"))
      cfg_->gridType = req.get_param_value("grid_type");
    if (req.has_param("prop_overlay"))
      cfg_->propOverlay =
          propOverlayFromString(req.get_param_value("prop_overlay"));
    if (req.has_param("wx_overlay"))
      cfg_->weatherOverlay =
          wxOverlayFromString(req.get_param_value("wx_overlay"));
    if (req.has_param("hub_mode")) {
      std::string m = req.get_param_value("hub_mode");
      if (m == "Master")
        cfg_->hubMode = HubMode::Master;
      else if (m == "Client")
        cfg_->hubMode = HubMode::Client;
      else
        cfg_->hubMode = HubMode::Off;
    }
    if (req.has_param("hub_ip"))
      cfg_->hubIp = req.get_param_value("hub_ip");
    if (req.has_param("hub_port"))
      cfg_->hubPort = StringUtils::safe_stoi(req.get_param_value("hub_port"));
    if (req.has_param("dx_enabled"))
      cfg_->dxClusterEnabled = req.get_param_value("dx_enabled") == "1";
    if (req.has_param("rbn_enabled"))
      cfg_->rbnEnabled = req.get_param_value("rbn_enabled") == "1";
    if (req.has_param("dx_host"))
      cfg_->dxClusterHost = req.get_param_value("dx_host");
    if (req.has_param("dx_port"))
      cfg_->dxClusterPort =
          StringUtils::safe_stoi(req.get_param_value("dx_port"));
    if (req.has_param("dx_login"))
      cfg_->dxClusterLogin = req.get_param_value("dx_login");
    if (req.has_param("dx_use_wsjtx"))
      cfg_->dxClusterUseWSJTX = req.get_param_value("dx_use_wsjtx") == "1";
    if (req.has_param("wsjtx_port"))
      cfg_->wsjtxPort =
          StringUtils::safe_stoi(req.get_param_value("wsjtx_port"));
    if (req.has_param("rig_host"))
      cfg_->rigHost = req.get_param_value("rig_host");
    if (req.has_param("rig_port"))
      cfg_->rigPort = StringUtils::safe_stoi(req.get_param_value("rig_port"));
    if (req.has_param("rig_auto_tune"))
      cfg_->rigAutoTune = req.get_param_value("rig_auto_tune") == "1";
    if (req.has_param("rot_host"))
      cfg_->rotatorHost = req.get_param_value("rot_host");
    if (req.has_param("rot_port"))
      cfg_->rotatorPort =
          StringUtils::safe_stoi(req.get_param_value("rot_port"));
    if (req.has_param("rot_auto_track"))
      cfg_->rotatorAutoTrack = req.get_param_value("rot_auto_track") == "1";
    if (req.has_param("rot_upover"))
      cfg_->rotatorUpover = req.get_param_value("rot_upover") == "1";
    if (req.has_param("qrz_user"))
      cfg_->qrzUsername = req.get_param_value("qrz_user");
    if (req.has_param("qrz_pass"))
      cfg_->qrzPassword = req.get_param_value("qrz_pass");
    if (req.has_param("rb_key"))
      cfg_->repeaterBookKey = req.get_param_value("rb_key");
    if (req.has_param("wl_key"))
      cfg_->winlinkKey = req.get_param_value("wl_key");
    if (req.has_param("gps_enabled"))
      cfg_->gpsEnabled = req.get_param_value("gps_enabled") == "1";
    if (req.has_param("rss_enabled"))
      cfg_->rssEnabled = req.get_param_value("rss_enabled") == "1";
    if (req.has_param("onta_filter"))
      cfg_->ontaFilter = req.get_param_value("onta_filter");
    if (req.has_param("spot_source")) {
      std::string s = req.get_param_value("spot_source");
      if (s == "PSK")
        cfg_->liveSpotSource = LiveSpotSource::PSK;
      else if (s == "RBN")
        cfg_->liveSpotSource = LiveSpotSource::RBN;
      else if (s == "WSPR")
        cfg_->liveSpotSource = LiveSpotSource::WSPR;
    }
    if (req.has_param("spot_max_age"))
      cfg_->liveSpotsMaxAge =
          StringUtils::safe_stoi(req.get_param_value("spot_max_age"));
    if (req.has_param("spot_bands"))
      cfg_->liveSpotsBands =
          (uint32_t)StringUtils::safe_stoi(req.get_param_value("spot_bands"));
    if (req.has_param("live_spots_of_de"))
      cfg_->liveSpotsOfDe = req.get_param_value("live_spots_of_de") == "1";
    if (req.has_param("live_spots_use_call"))
      cfg_->liveSpotsUseCall = req.get_param_value("live_spots_use_call") == "1";
    if (req.has_param("pskr_proxy_url"))
      cfg_->pskrProxyUrl = req.get_param_value("pskr_proxy_url");
    if (req.has_param("brightness"))
      cfg_->brightness =
          StringUtils::safe_stoi(req.get_param_value("brightness"));
    if (req.has_param("brightness_schedule"))
      cfg_->brightnessSchedule =
          req.get_param_value("brightness_schedule") == "1";
    if (req.has_param("dim_hour"))
      cfg_->dimHour = StringUtils::safe_stoi(req.get_param_value("dim_hour"));
    if (req.has_param("dim_min"))
      cfg_->dimMinute = StringUtils::safe_stoi(req.get_param_value("dim_min"));
    if (req.has_param("bright_hour"))
      cfg_->brightHour =
          StringUtils::safe_stoi(req.get_param_value("bright_hour"));
    if (req.has_param("bright_min"))
      cfg_->brightMinute =
          StringUtils::safe_stoi(req.get_param_value("bright_min"));
    if (req.has_param("rotation_interval"))
      cfg_->rotationIntervalS =
          StringUtils::safe_stoi(req.get_param_value("rotation_interval"));
    if (req.has_param("sync_rotation"))
      cfg_->syncRotation = req.get_param_value("sync_rotation") == "1";

    if (req.has_param("watchlist")) {
      std::string w = req.get_param_value("watchlist");
      cfg_->watchlist.clear();
      std::stringstream ss(w);
      std::string item;
      while (std::getline(ss, item)) {
        item = StringUtils::trim(item);
        if (!item.empty())
          cfg_->watchlist.push_back(item);
      }
    }

    auto parsePane = [&](const std::string &val, std::vector<WidgetType> &rot) {
      if (val.empty())
        return;
      rot.clear();
      std::stringstream ss(val);
      std::string id;
      while (std::getline(ss, id, ',')) {
        rot.push_back(widgetTypeFromString(id, WidgetType::SOLAR));
      }
      if (rot.empty())
        rot.push_back(WidgetType::SOLAR);
    };
    if (req.has_param("pane0"))
      parsePane(req.get_param_value("pane0"), cfg_->pane1Rotation);
    if (req.has_param("pane1"))
      parsePane(req.get_param_value("pane1"), cfg_->pane2Rotation);
    if (req.has_param("pane2"))
      parsePane(req.get_param_value("pane2"), cfg_->pane3Rotation);
    if (req.has_param("pane3"))
      parsePane(req.get_param_value("pane3"), cfg_->pane4Rotation);
    if (req.has_param("pane4"))
      parsePane(req.get_param_value("pane4"), cfg_->pane5Rotation);
    if (req.has_param("pane5"))
      parsePane(req.get_param_value("pane5"), cfg_->pane6Rotation);

    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_rss",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("enabled")) {
              cfg_->rssEnabled = req.get_param_value("enabled") == "1";
              if (cfgMgr_)
                cfgMgr_->save(*cfg_);
              if (reloadFlag_)
                reloadFlag_->store(true, std::memory_order_release);
              res.set_content("ok", "text/plain");
            } else {
              res.status = 400;
            }
          });

  svr.Get("/set_mapcolor",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("key") && req.has_param("color")) {
              std::string key = req.get_param_value("key");
              std::string color = req.get_param_value("color");
              cfg_->colorOverrides[key] = hexToColor(color);
              if (cfgMgr_)
                cfgMgr_->save(*cfg_);
              if (reloadFlag_)
                reloadFlag_->store(true, std::memory_order_release);
              res.set_content("ok", "text/plain");
            } else {
              res.status = 400;
            }
          });

  svr.Post("/set_adif",
           [this](const httplib::Request &req, httplib::Response &res) {
             if (req.has_file("adif")) {
               const auto &file = req.get_file_value("adif");
               std::filesystem::path path = cfgMgr_->configDir() / "upload.adi";
               std::ofstream ofs(path, std::ios::binary);
               if (ofs) {
                 ofs.write(file.content.data(), file.content.size());
                 ofs.close();
                 if (adifProvider_) {
                   adifProvider_->fetch(path);
                 }
                 res.set_content("ok", "text/plain");
               } else {
                 res.status = 500;
               }
             } else {
               res.status = 400;
             }
           });

  svr.Get("/api/display/status", [this](const httplib::Request &,
                                        httplib::Response &res) {
    nlohmann::json j;
    j["fps"] = state_ ? std::to_string(state_->fps).substr(0, 4) : "0";
    j["uptime"] =
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now() - startTime_)
                           .count()) +
        "s";
    j["power"] = displayPower_ ? (displayPower_->getPower() ? "on" : "off") : "unknown";
    j["method"] = displayPower_ ? displayPower_->getSelectedMethodName() : "none";
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/api/hub/fetch", [this](const httplib::Request &req,
                                   httplib::Response &res) {
    if (!cfg_ || cfg_->hubMode != HubMode::Master) {
      res.status = 403;
      return;
    }
    std::string targetUrl = base64Decode(req.get_param_value("url"));
    if (isPrivateOrLoopbackUrl(targetUrl)) {
      res.status = 403;
      return;
    }
    std::promise<std::string> prom;
    auto fut = prom.get_future();
    netMgr_->fetchAsync(
        targetUrl, [&prom](std::string b) { prom.set_value(std::move(b)); },
        3600);
    if (fut.wait_for(std::chrono::seconds(20)) == std::future_status::timeout) {
      res.status = 504;
      return;
    }
    std::string body = fut.get();
    if (body.empty()) {
      res.status = 502;
      return;
    }
    res.set_content(body, "application/octet-stream");
  });

  // ============================================================
  // Phase 1 — Live Web View
  // ============================================================

  svr.Get("/live/status", [this](const httplib::Request &,
                                 httplib::Response &res) {
    nlohmann::json j;
    j["liveWebEnabled"] = liveWebEnabled_;
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/live/touch", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    int x = StringUtils::safe_stoi(req.get_param_value("x"));
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    int outW = LOGICAL_WIDTH, outH = LOGICAL_HEIGHT;
    if (renderer_)
      SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    int lx = (outW > 0) ? (x * LOGICAL_WIDTH / outW) : x;
    int ly = (outH > 0) ? (y * LOGICAL_HEIGHT / outH) : y;
    SDL_Event e{};
    e.type = AE_BASE_EVENT + AE_TOUCH;
    e.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(lx));
    e.user.data2 = reinterpret_cast<void *>(static_cast<intptr_t>(ly));
    SDL_PushEvent(&e);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/live/wheel", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    SDL_Event e{};
    e.type = SDL_MOUSEWHEEL;
    e.wheel.y = y;
    SDL_PushEvent(&e);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/live/key", [this](const httplib::Request &req,
                              httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    std::string key = req.get_param_value("key");
    bool ctrl = req.get_param_value("ctrl") == "1";
    bool shift = req.get_param_value("shift") == "1";
    SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
    SDL_Keymod mod = static_cast<SDL_Keymod>(
        (ctrl ? KMOD_CTRL : 0) | (shift ? KMOD_SHIFT : 0));
    SDL_Event down{}, up{};
    down.type = SDL_KEYDOWN;
    down.key.keysym.scancode = sc;
    down.key.keysym.mod = mod;
    up = down;
    up.type = SDL_KEYUP;
    SDL_PushEvent(&down);
    SDL_PushEvent(&up);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/live/mouse", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    int x = StringUtils::safe_stoi(req.get_param_value("x"));
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    int outW = LOGICAL_WIDTH, outH = LOGICAL_HEIGHT;
    if (renderer_)
      SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    int px = (outW > 0) ? (x * outW / LOGICAL_WIDTH) : x;
    int py = (outH > 0) ? (y * outH / LOGICAL_HEIGHT) : y;
    SDL_Event e{};
    e.type = SDL_MOUSEMOTION;
    e.motion.x = px;
    e.motion.y = py;
    SDL_PushEvent(&e);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/live", [](const httplib::Request &, httplib::Response &res) {
    static const std::string html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>HamClock Live View</title>
  <style>
    body { background: #000; margin: 0; display: flex; flex-direction: column; align-items: center; }
    #info { color: #0f0; font-family: monospace; font-size: 12px; padding: 4px; }
    #feed { max-width: 100%; display: block; cursor: crosshair; }
  </style>
</head>
<body>
  <div id="info">Click map to interact &bull; Scroll to zoom &bull; <a href="/" style="color:#0f0">Config</a></div>
  <img id="feed" src="/stream.mjpeg">
  <script>
    const img = document.getElementById('feed');
    img.addEventListener('click', e => {
      const r = img.getBoundingClientRect();
      const x = Math.round((e.clientX - r.left) * img.naturalWidth / r.width);
      const y = Math.round((e.clientY - r.top) * img.naturalHeight / r.height);
      fetch('/live/touch?x=' + x + '&y=' + y + '&button=1&shift=' + (e.shiftKey ? 1 : 0));
    });
    img.addEventListener('wheel', e => {
      e.preventDefault();
      fetch('/live/wheel?y=' + (e.deltaY > 0 ? -1 : 1));
    }, {passive: false});
    document.addEventListener('keydown', e => {
      fetch('/live/key?key=' + encodeURIComponent(e.key) +
            '&ctrl=' + (e.ctrlKey ? 1 : 0) + '&shift=' + (e.shiftKey ? 1 : 0));
    });
    img.addEventListener('mousemove', e => {
      const r = img.getBoundingClientRect();
      const x = Math.round((e.clientX - r.left) * img.naturalWidth / r.width);
      const y = Math.round((e.clientY - r.top) * img.naturalHeight / r.height);
      fetch('/live/mouse?x=' + x + '&y=' + y);
    });
  </script>
</body>
</html>)HTML";
    res.set_content(html, "text/html");
  });

  svr.Get("/stream.mjpeg", [this](const httplib::Request &,
                                  httplib::Response &res) {
    if (!frameCapture_ || !liveWebEnabled_) {
      res.status = 503;
      return;
    }
    frameCapture_->addSubscriber();
    uint64_t seq = 0;
    res.set_content_provider(
        "multipart/x-mixed-replace;boundary=hamclock",
        [this, seq](size_t /*offset*/,
                    httplib::DataSink &sink) mutable -> bool {
          if (!sink.is_writable())
            return false;
          uint64_t outSeq = 0;
          auto frame = frameCapture_->waitFrame(seq, 5000, outSeq);
          if (frame.empty())
            return sink.is_writable();
          seq = outSeq;
          std::string hdr =
              "--hamclock\r\nContent-Type: image/jpeg\r\nContent-Length: " +
              std::to_string(frame.size()) + "\r\n\r\n";
          if (!sink.write(hdr.data(), hdr.size()))
            return false;
          if (!sink.write(reinterpret_cast<const char *>(frame.data()),
                          frame.size()))
            return false;
          return sink.write("\r\n", 2);
        },
        [this](bool) {
          if (frameCapture_)
            frameCapture_->removeSubscriber();
        });
  });

  // ============================================================
  // Phase 2 — Legacy Data Getters
  // ============================================================

  svr.Get("/get_config.txt", [this](const httplib::Request &,
                                    httplib::Response &res) {
    std::ostringstream oss;
    oss << "Callsign   " << cfg_->callsign << "\n";
    oss << "Grid       " << cfg_->grid << "\n";
    oss << "Theme      " << cfg_->theme << "\n";
    oss << "Lat        " << cfg_->lat << "\n";
    oss << "Lon        " << cfg_->lon << "\n";
    oss << "CORSProxy  " << cfg_->corsProxyUrl << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_time.txt",
          [](const httplib::Request &, httplib::Response &res) {
            std::time_t now = std::time(nullptr);
            struct tm utc {};
            Astronomy::portable_gmtime(&now, &utc);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                          utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                          utc.tm_hour, utc.tm_min, utc.tm_sec);
            std::ostringstream oss;
            oss << "Clock_UTC  " << buf << "\n";
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_de.txt", [this](const httplib::Request &,
                                httplib::Response &res) {
    std::ostringstream oss;
    oss << "DE_Callsign  " << state_->deCallsign << "\n";
    oss << "DE_Grid      " << state_->deGrid << "\n";
    oss << "DE_Lat       " << state_->deLocation.lat << "\n";
    oss << "DE_Lon       " << state_->deLocation.lon << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_spacewx.txt", [this](const httplib::Request &,
                                     httplib::Response &res) {
    if (!solar_) {
      res.status = 503;
      return;
    }
    SolarData d = solar_->get();
    std::ostringstream oss;
    oss << "SFI       " << d.sfi << "\n";
    oss << "SSN       " << d.sunspot_number << "\n";
    oss << "Kp        " << d.k_index << "\n";
    oss << "Ap        " << d.a_index << "\n";
    oss << "Bz        " << d.bz << "\n";
    oss << "Bt        " << d.bt << "\n";
    oss << "Wind      " << d.solar_wind_speed << "\n";
    oss << "Density   " << d.solar_wind_density << "\n";
    oss << "Aurora    " << d.aurora << "\n";
    oss << "Dst       " << d.dst << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_sys.txt", [this](const httplib::Request &,
                                 httplib::Response &res) {
    std::ostringstream oss;
    if (cpu_) {
      oss << "CPU_Temp_C  " << cpu_->getTemperature() << "\n";
      oss << "CPU_Temp_F  " << cpu_->getTemperatureF() << "\n";
    }
    auto upSec = std::chrono::duration_cast<std::chrono::seconds>(
                     std::chrono::system_clock::now() - startTime_)
                     .count();
    oss << "Uptime_s    " << upSec << "\n";
    if (state_)
      oss << "FPS         " << state_->fps << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_contests.txt", [this](const httplib::Request &,
                                      httplib::Response &res) {
    if (!contests_) {
      res.status = 503;
      return;
    }
    ContestData cd = contests_->get();
    std::ostringstream oss;
    for (const auto &c : cd.contests)
      oss << c.title << "  " << c.dateDesc << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  {
    auto dxSpotsHandler = [this](const httplib::Request &,
                                 httplib::Response &res) {
      if (!dxc_) {
        res.status = 503;
        return;
      }
      auto snap = dxc_->snapshot();
      std::ostringstream oss;
      int count = 0;
      for (auto it = snap->spots.rbegin();
           it != snap->spots.rend() && count < 20; ++it, ++count) {
        oss << std::left << std::setw(12) << it->txCall << std::setw(10)
            << it->mode << std::setw(12) << it->freqKhz << it->rxCall << "\n";
      }
      res.set_content(oss.str(), "text/plain");
    };
    svr.Get("/get_dxpots.txt", dxSpotsHandler);
    svr.Get("/get_dxspots.txt", dxSpotsHandler);
  }

  svr.Get("/get_livespots.txt", [this](const httplib::Request &,
                                       httplib::Response &res) {
    if (!spots_) {
      res.status = 503;
      return;
    }
    auto snap = spots_->snapshot();
    std::ostringstream oss;
    int count = 0;
    for (const auto &s : snap->spots) {
      if (count++ >= 20)
        break;
      oss << std::left << std::setw(12) << s.senderCallsign << std::setw(12)
          << s.receiverGrid << s.freqKhz << "\n";
    }
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_livestats.txt", [this](const httplib::Request &,
                                       httplib::Response &res) {
    if (!spots_) {
      res.status = 503;
      return;
    }
    auto snap = spots_->snapshot();
    std::ostringstream oss;
    for (int i = 0; i < kNumBands; ++i)
      oss << kBands[i].name << "  " << snap->bandCounts[i] << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_satellites.txt", [this](const httplib::Request &,
                                        httplib::Response &res) {
    if (!satMgr_) {
      res.status = 503;
      return;
    }
    auto sats = satMgr_->getSatellites();
    std::ostringstream oss;
    for (const auto &s : sats)
      oss << s.name << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_ontheair.txt", [this](const httplib::Request &,
                                      httplib::Response &res) {
    if (!activityStore_) {
      res.status = 503;
      return;
    }
    ActivityData ad = activityStore_->get();
    std::ostringstream oss;
    for (const auto &s : ad.ontaSpots)
      oss << std::left << std::setw(12) << s.call << std::setw(8) << s.program
          << std::setw(10) << s.ref << s.freqKhz << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_dxpeds.txt", [this](const httplib::Request &,
                                    httplib::Response &res) {
    if (!activityStore_) {
      res.status = 503;
      return;
    }
    ActivityData ad = activityStore_->get();
    std::ostringstream oss;
    for (const auto &d : ad.dxpeds)
      oss << std::left << std::setw(12) << d.call << d.location << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_capture", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (!frameCapture_) {
      res.status = 503;
      return;
    }
    
    // Increment subscribers so FrameCapture::capture() actually runs on the main thread
    frameCapture_->addSubscriber();
    
    std::string seqParam = req.get_param_value("seq");
    uint64_t afterSeq = 0;
    if (seqParam.empty()) {
      // If no seq is provided, wait for the NEXT frame after the current one.
      // This ensures we don't get a stale cached frame from when nobody was
      // watching.
      afterSeq = frameCapture_->latestSeq();
    } else {
      afterSeq = static_cast<uint64_t>(StringUtils::safe_stoi(seqParam));
    }
    uint64_t outSeq = 0;
    
    // Increased timeout to 1000ms to allow for RPi 3B scheduling and encoding
    auto frame = frameCapture_->waitFrame(afterSeq, 1000, outSeq);
    
    frameCapture_->removeSubscriber();

    if (frame.empty()) {
      res.status = 503;
      return;
    }
    res.set_header("X-Frame-Seq", std::to_string(outSeq));
    res.set_content(
        std::string(reinterpret_cast<const char *>(frame.data()), frame.size()),
        "image/jpeg");
  });

  svr.Get("/get_dx.txt", [this](const httplib::Request &,
                                httplib::Response &res) {
    if (!state_->dxActive) {
      res.set_content("DX not set\n", "text/plain");
      return;
    }
    std::ostringstream oss;
    oss << "DX_Grid      " << state_->dxGrid << "\n";
    oss << "DX_Lat       " << state_->dxLocation.lat << "\n";
    oss << "DX_Lon       " << state_->dxLocation.lon << "\n";
    double distKm =
        Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
    double bearing =
        Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
    oss << "DX_Dist_km   " << static_cast<int>(distKm) << "\n";
    oss << "DX_Bearing   " << static_cast<int>(bearing) << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  // ============================================================
  // Phase 3 — Legacy Control Setters
  // ============================================================

  svr.Get("/set_screenlock", [this](const httplib::Request &req,
                                    httplib::Response &res) {
    screenLocked_ = (req.get_param_value("lock") == "on");
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_mappos", [this](const httplib::Request &req,
                                httplib::Response &res) {
    double lat = StringUtils::safe_stod(req.get_param_value("lat"));
    double lon = StringUtils::safe_stod(req.get_param_value("lon"));
    std::string target = req.get_param_value("target");
    std::string grid = Astronomy::latLonToGrid(lat, lon);
    if (target == "dx") {
      state_->dxLocation = {lat, lon};
      state_->dxGrid = grid;
      state_->dxActive = true;
    } else {
      state_->deLocation = {lat, lon};
      state_->deGrid = grid;
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_displayOnOff", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    if (!displayPower_) {
      res.status = 503;
      return;
    }
    bool on = req.has_param("on") || req.get_param_value("state") != "off";
    if (req.has_param("off"))
      on = false;
    displayPower_->setPower(on);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_newde", [this](const httplib::Request &req,
                               httplib::Response &res) {
    double lat = 0, lon = 0;
    if (req.has_param("grid")) {
      if (!Astronomy::gridToLatLon(req.get_param_value("grid"), lat, lon)) {
        res.status = 400;
        return;
      }
    } else {
      lat = StringUtils::safe_stod(req.get_param_value("lat"));
      lon = StringUtils::safe_stod(req.get_param_value("lon"));
    }
    state_->deLocation = {lat, lon};
    state_->deGrid = Astronomy::latLonToGrid(lat, lon);
    cfg_->lat = lat;
    cfg_->lon = lon;
    cfg_->grid = state_->deGrid;
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_newdx", [this](const httplib::Request &req,
                               httplib::Response &res) {
    double lat = 0, lon = 0;
    if (req.has_param("grid")) {
      if (!Astronomy::gridToLatLon(req.get_param_value("grid"), lat, lon)) {
        res.status = 400;
        return;
      }
    } else {
      lat = StringUtils::safe_stod(req.get_param_value("lat"));
      lon = StringUtils::safe_stod(req.get_param_value("lon"));
    }
    state_->dxLocation = {lat, lon};
    state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
    state_->dxActive = true;
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_cluster", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (req.has_param("host"))
      cfg_->dxClusterHost = req.get_param_value("host");
    if (req.has_param("port"))
      cfg_->dxClusterPort =
          StringUtils::safe_stoi(req.get_param_value("port"));
    if (req.has_param("user"))
      cfg_->dxClusterLogin = req.get_param_value("user");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_title", [this](const httplib::Request &req,
                               httplib::Response &res) {
    if (!req.has_param("call")) {
      res.status = 400;
      return;
    }
    cfg_->callsign = req.get_param_value("call");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

#ifdef __linux__
  svr.Get("/set_time", [](const httplib::Request &req,
                          httplib::Response &res) {
    struct timeval tv {};
    if (req.has_param("unix")) {
      tv.tv_sec =
          static_cast<time_t>(StringUtils::safe_stoi(req.get_param_value("unix")));
    } else if (req.has_param("ISO")) {
      std::string iso = req.get_param_value("ISO");
      struct tm t {};
      if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d", &t.tm_year, &t.tm_mon,
                      &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) >= 6) {
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        tv.tv_sec = Astronomy::portable_timegm(&t);
      } else {
        res.status = 400;
        return;
      }
    } else if (req.has_param("Now")) {
      gettimeofday(&tv, nullptr);
    } else if (req.has_param("change")) {
      gettimeofday(&tv, nullptr);
      tv.tv_sec += StringUtils::safe_stoi(req.get_param_value("change"));
    } else {
      res.status = 400;
      return;
    }
    if (settimeofday(&tv, nullptr) != 0) {
      res.status = 500;
      return;
    }
    res.set_content("ok", "text/plain");
  });
#endif

  svr.Get("/set_alarm", [this](const httplib::Request &req,
                               httplib::Response &res) {
    if (req.has_param("state"))
      cfg_->alarmArmed = (req.get_param_value("state") == "on");
    if (req.has_param("time")) {
      std::string t = req.get_param_value("time");
      int hh = 0, mm = 0;
      if (std::sscanf(t.c_str(), "%d:%d", &hh, &mm) == 2) {
        cfg_->alarmTimeHH = hh;
        cfg_->alarmTimeMM = mm;
      }
    }
    if (req.has_param("utc"))
      cfg_->alarmUtc = (req.get_param_value("utc") == "1");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_once_alarm", [this](const httplib::Request &req,
                                    httplib::Response &res) {
    if (req.has_param("state"))
      cfg_->onceAlarmArmed = (req.get_param_value("state") == "on");
    if (req.has_param("time"))
      cfg_->onceAlarmTime = req.get_param_value("time");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_stopwatch", [](const httplib::Request &req,
                               httplib::Response &res) {
    SDL_Event e{};
    if (req.has_param("run")) {
      e.type = SDL_USER_EVENT_STOPWATCH_RUN;
    } else if (req.has_param("stop")) {
      e.type = SDL_USER_EVENT_STOPWATCH_STOP;
    } else if (req.has_param("reset")) {
      e.type = SDL_USER_EVENT_STOPWATCH_RESET;
    } else if (req.has_param("countdown")) {
      e.type = SDL_USER_EVENT_STOPWATCH_COUNTDOWN;
      e.user.code = StringUtils::safe_stoi(req.get_param_value("countdown"));
    } else {
      res.status = 400;
      return;
    }
    SDL_PushEvent(&e);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/get_satellite.txt", [this](const httplib::Request &,
                                       httplib::Response &res) {
    if (!satMgr_) {
      res.status = 503;
      return;
    }
    std::string name = satMgr_->getTrackedSatellite();
    if (name.empty()) {
      res.set_content("Satellite  none\n", "text/plain");
      return;
    }
    std::ostringstream oss;
    oss << "Satellite  " << name << "\n";
    const SatelliteTLE *tle = satMgr_->findByName(name);
    if (tle) {
      Satellite sat(*tle);
      sat.setObserver(state_->deLocation.lat, state_->deLocation.lon);
      SatObservation obs = sat.predict();
      oss << "Az         " << static_cast<int>(obs.azimuth) << "\n";
      oss << "El         " << static_cast<int>(obs.elevation) << "\n";
      oss << "Range_km   " << static_cast<int>(obs.range) << "\n";
      oss << "Visible    " << (obs.visible ? "yes" : "no") << "\n";
    }
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/set_satname", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (!satMgr_) {
      res.status = 503;
      return;
    }
    std::string name = req.get_param_value("name");
    satMgr_->trackSatellite((name == "none") ? "" : name);
    cfg_->selectedSatellite = satMgr_->getTrackedSatellite();
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_sattle", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!satMgr_) {
      res.status = 503;
      return;
    }
    std::string name = req.get_param_value("name");
    std::string line1 = req.get_param_value("line1");
    std::string line2 = req.get_param_value("line2");
    if (name.empty() || line1.empty() || line2.empty()) {
      res.status = 400;
      return;
    }
    SatelliteTLE tle;
    tle.name = name;
    tle.line1 = line1;
    tle.line2 = line2;
    satMgr_->addCustomTLE(tle);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/get_active_pane.txt", [this](const httplib::Request &,
                                         httplib::Response &res) {
    if (!panes_ || panes_->empty()) {
      res.status = 503;
      return;
    }
    std::ostringstream oss;
    for (size_t i = 0; i < panes_->size(); ++i)
      oss << "Pane" << (i + 1) << "  " << (*panes_)[i]->getDisplayName()
          << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_pane.txt", [this](const httplib::Request &req,
                                  httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("pane")) - 1;
    if (!panes_ || idx < 0 || idx >= (int)panes_->size()) {
      res.status = 404;
      return;
    }
    auto &pane = (*panes_)[idx];
    std::ostringstream oss;
    oss << "Active  " << pane->getDisplayName() << "\n";
    for (auto t : pane->getRotation())
      oss << "Widget  " << widgetTypeDisplayName(t) << "\n";
    res.set_content(oss.str(), "text/plain");
  });

  // Surgical map configuration endpoints (no full dashboard reset)
  svr.Get("/set_projection", [this](const httplib::Request &req,
                                    httplib::Response &res) {
    if (req.has_param("type")) {
      cfg_->projection = req.get_param_value("type");
      if (cfgMgr_)
        cfgMgr_->save(*cfg_);
      if (mapReloadFlag_)
        mapReloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else
      res.status = 400;
  });

  svr.Get("/set_prop_overlay", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    if (req.has_param("type")) {
      cfg_->propOverlay = propOverlayFromString(req.get_param_value("type"));
      if (cfgMgr_)
        cfgMgr_->save(*cfg_);
      if (mapReloadFlag_)
        mapReloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else
      res.status = 400;
  });

  svr.Get("/set_wx_overlay", [this](const httplib::Request &req,
                                    httplib::Response &res) {
    if (req.has_param("type")) {
      cfg_->weatherOverlay = wxOverlayFromString(req.get_param_value("type"));
      if (cfgMgr_)
        cfgMgr_->save(*cfg_);
      if (mapReloadFlag_)
        mapReloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else
      res.status = 400;
  });

  svr.Get("/set_pane", [this](const httplib::Request &req,
                              httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("pane")) - 1;
    if (!panes_ || idx < 0 || idx >= (int)panes_->size()) {
      res.status = 404;
      return;
    }
    std::string action = req.get_param_value("action");
    std::string widget = req.get_param_value("widget");
    auto &pane = (*panes_)[idx];
    if (action == "next") {
      pane->forceAdvance();
    } else if (action == "add" && !widget.empty()) {
      auto rot = pane->getRotation();
      WidgetType wt = widgetTypeFromString(widget, WidgetType::SOLAR);
      if (std::find(rot.begin(), rot.end(), wt) == rot.end()) {
        rot.push_back(wt);
        pane->setRotation(rot, cfg_->rotationIntervalS, cfg_->syncRotation);
      }
    } else if (action == "remove" && !widget.empty()) {
      auto rot = pane->getRotation();
      WidgetType wt = widgetTypeFromString(widget, WidgetType::SOLAR);
      rot.erase(std::remove(rot.begin(), rot.end(), wt), rot.end());
      if (!rot.empty())
        pane->setRotation(rot, cfg_->rotationIntervalS, cfg_->syncRotation);
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_displayTimes", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    if (req.has_param("brightness_schedule"))
      cfg_->brightnessSchedule =
          req.get_param_value("brightness_schedule") == "1";
    if (req.has_param("dim_hour"))
      cfg_->dimHour = StringUtils::safe_stoi(req.get_param_value("dim_hour"));
    if (req.has_param("dim_min"))
      cfg_->dimMinute =
          StringUtils::safe_stoi(req.get_param_value("dim_min"));
    if (req.has_param("bright_hour"))
      cfg_->brightHour =
          StringUtils::safe_stoi(req.get_param_value("bright_hour"));
    if (req.has_param("bright_min"))
      cfg_->brightMinute =
          StringUtils::safe_stoi(req.get_param_value("bright_min"));
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (brightnessMgr_) {
      brightnessMgr_->setScheduleEnabled(cfg_->brightnessSchedule);
      brightnessMgr_->setDimTime(cfg_->dimHour, cfg_->dimMinute);
      brightnessMgr_->setBrightTime(cfg_->brightHour, cfg_->brightMinute);
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_mapcenter", [this](const httplib::Request &req,
                                   httplib::Response &res) {
    if (!req.has_param("lon")) {
      res.status = 400;
      return;
    }
    cfg_->mapCenterLon = StringUtils::safe_stod(req.get_param_value("lon"));
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_panzoom", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (req.has_param("x"))
      cfg_->mapPanX = StringUtils::safe_stoi(req.get_param_value("x"));
    if (req.has_param("y"))
      cfg_->mapPanY = StringUtils::safe_stoi(req.get_param_value("y"));
    if (req.has_param("zoom"))
      cfg_->mapZoom = StringUtils::safe_stod(req.get_param_value("zoom"));
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_rotation", [this](const httplib::Request &req,
                                  httplib::Response &res) {
    if (req.has_param("interval"))
      cfg_->rotationIntervalS =
          StringUtils::safe_stoi(req.get_param_value("interval"));
    if (req.has_param("sync"))
      cfg_->syncRotation = (req.get_param_value("sync") == "1");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (panes_) {
      for (auto &p : *panes_)
        p->setRotation(p->getRotation(), cfg_->rotationIntervalS,
                       cfg_->syncRotation);
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_rotator", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (req.has_param("host"))
      cfg_->rotatorHost = req.get_param_value("host");
    if (req.has_param("port"))
      cfg_->rotatorPort =
          StringUtils::safe_stoi(req.get_param_value("port"));
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/screen", [this](const httplib::Request &req,
                            httplib::Response &res) {
    if (req.has_param("blank") || req.has_param("prevent")) {
      SDL_Event e{};
      e.type = SDL_USER_EVENT_BLOCK_SLEEP;
      e.user.code = req.has_param("prevent") ? 1 : 0;
      SDL_PushEvent(&e);
      res.set_content("ok", "text/plain");
    } else {
      nlohmann::json j;
      j["preventSleep"] = cfg_->preventSleep;
      j["saverEnabled"] = (cfg_->idleMinutes > 0);
      j["displayPower"] = displayPower_ ? displayPower_->getPower() : true;
      res.set_content(j.dump(), "application/json");
    }
  });

  // ============================================================
  // Phase 4 — Propagation, Debug, and Reload
  // ============================================================

  svr.Get("/api/propagation/voacap",
          [](const httplib::Request &, httplib::Response &res) {
            static const std::string body =
                R"({"type":"propagation","service":"voacap",)"
                R"("colormaps":["jet","viridis","plasma"],)"
                R"("bands":["80m","40m","20m","17m","15m","12m","10m"],)"
                R"("modes":["CW","SSB","AM","FT8"],"available":true})";
            res.set_content(body, "application/json");
          });

  svr.Get("/api/propagation/muf_rt",
          [](const httplib::Request &, httplib::Response &res) {
            static const std::string body =
                R"({"type":"propagation","service":"muf_rt",)"
                R"("provider":"KC2G",)"
                R"("description":"Real-time MUF from ionosonde network",)"
                R"("available":true})";
            res.set_content(body, "application/json");
          });

  svr.Post("/api/reload", [this](const httplib::Request &,
                                 httplib::Response &res) {
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Post("/api/display/power", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    if (!displayPower_) {
      res.status = 503;
      return;
    }
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.body);
    } catch (...) {
      res.status = 400;
      return;
    }
    std::string st = body.value("state", "on");
    bool on = (st != "off");
    displayPower_->setPower(on);
    nlohmann::json j;
    j["state"] = on ? "on" : "off";
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/debug/performance", [this](const httplib::Request &,
                                       httplib::Response &res) {
    nlohmann::json j;
    j["fps"] = state_ ? state_->fps : 0.0f;
    j["running_since"] =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - startTime_)
            .count();
    if (cpu_) {
      j["cpu_temp_c"] = cpu_->getTemperature();
    }
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/debug/health", [this](const httplib::Request &,
                                  httplib::Response &res) {
    nlohmann::json j;
    if (state_) {
      for (const auto &[name, st] : state_->services) {
        nlohmann::json sj;
        sj["ok"] = st.ok;
        sj["lastError"] = st.lastError;
        if (st.lastSuccess != std::chrono::system_clock::time_point{}) {
          std::time_t t = std::chrono::system_clock::to_time_t(st.lastSuccess);
          char buf[32];
          struct tm utc {};
          Astronomy::portable_gmtime(&t, &utc);
          std::snprintf(buf, sizeof(buf), "%02d:%02d:%02dZ", utc.tm_hour,
                        utc.tm_min, utc.tm_sec);
          sj["lastSuccess"] = buf;
        } else {
          sj["lastSuccess"] = nullptr;
        }
        j[name] = sj;
      }
    }
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/debug/logs",
          [](const httplib::Request &, httplib::Response &res) {
            res.set_content("[]", "application/json");
          });

#ifdef ENABLE_DEBUG_API
  svr.Get("/debug/widgets", [](const httplib::Request &,
                               httplib::Response &res) {
    nlohmann::json j = nlohmann::json::array();
    auto snapshot = UIRegistry::getInstance().getSnapshot();
    for (const auto &[id, info] : snapshot) {
      nlohmann::json w;
      w["id"] = id;
      w["name"] = info.name;
      w["rect"] = {{"x", info.rect.x}, {"y", info.rect.y},
                   {"w", info.rect.w}, {"h", info.rect.h}};
      j.push_back(w);
    }
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/debug/click", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    int x = StringUtils::safe_stoi(req.get_param_value("x"));
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    int outW = LOGICAL_WIDTH, outH = LOGICAL_HEIGHT;
    if (renderer_)
      SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    int px = (outW > 0) ? (x * outW / LOGICAL_WIDTH) : x;
    int py = (outH > 0) ? (y * outH / LOGICAL_HEIGHT) : y;
    SDL_Event down{}, up{};
    down.type = SDL_MOUSEBUTTONDOWN;
    down.button.button = SDL_BUTTON_LEFT;
    down.button.x = px;
    down.button.y = py;
    up = down;
    up.type = SDL_MOUSEBUTTONUP;
    SDL_PushEvent(&down);
    SDL_PushEvent(&up);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/debug/keypress", [](const httplib::Request &req,
                                httplib::Response &res) {
    std::string key = req.get_param_value("key");
    SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
    SDL_Event down{}, up{};
    down.type = SDL_KEYDOWN;
    down.key.keysym.scancode = sc;
    up = down;
    up.type = SDL_KEYUP;
    SDL_PushEvent(&down);
    SDL_PushEvent(&up);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/debug/watchlist/add", [this](const httplib::Request &req,
                                         httplib::Response &res) {
    std::string call = req.get_param_value("call");
    if (call.empty()) {
      res.status = 400;
      return;
    }
    cfg_->watchlist.push_back(call);
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/debug/store/set_solar", [this](const httplib::Request &req,
                                           httplib::Response &res) {
    if (!solar_) {
      res.status = 503;
      return;
    }
    SolarData d = solar_->get();
    if (req.has_param("sfi"))
      d.sfi = StringUtils::safe_stoi(req.get_param_value("sfi"));
    if (req.has_param("kp"))
      d.k_index =
          static_cast<float>(StringUtils::safe_stod(req.get_param_value("kp")));
    if (req.has_param("ssn"))
      d.sunspot_number = StringUtils::safe_stoi(req.get_param_value("ssn"));
    solar_->set(d);
    res.set_content("ok", "text/plain");
  });
#endif

  svr.listen("0.0.0.0", port_);
#endif
}
