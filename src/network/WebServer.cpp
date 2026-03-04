#include "WebServer.h"
#include "NetworkManager.h"
#include "../ui/PaneContainer.h"

#include <SDL.h>

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/StringUtils.h"
#include "../core/WatchlistStore.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../core/Logger.h"

#include "../core/Astronomy.h"
#include "../core/DisplayPower.h"
#include "FrameCapture.h"
#include <future>
#include <iomanip>
#include <sstream>

#include "../core/ActivityData.h"
#include "../core/BrightnessManager.h"
#include "../core/CPUMonitor.h"
#include "../core/MemoryMonitor.h"
#include "../core/WeatherData.h"
#include "../services/RotatorService.h"
#ifdef __linux__
#include <sys/time.h>
#endif
#include "../core/ContestData.h"
#include "../core/DXClusterData.h"
#include "../core/LiveSpotData.h"
#include "../core/SatelliteManager.h"
#ifdef ENABLE_DEBUG_API
#include "../core/UIRegistry.h"
#endif

// Platform includes for interface enumeration (private-IP gate)
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

// Returns true if the URL targets a private/loopback address (SSRF guard)
// Returns true if all non-loopback interfaces have private/link-local
// addresses. If ANY interface has a public address this returns false and the
// web server should not start.
static bool isHostOnPrivateNetwork() {
#if defined(__linux__) || defined(__APPLE__)
  struct ifaddrs *ifap = nullptr;
  if (getifaddrs(&ifap) != 0)
    return true; // can't determine — allow rather than block on error

  bool foundNonLoopback = false;
  bool allPrivate = true;

  for (struct ifaddrs *ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr)
      continue;
    // IPv4
    if (ifa->ifa_addr->sa_family == AF_INET) {
      auto *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
      uint32_t ip = ntohl(sa->sin_addr.s_addr);
      if (ip == 0x7F000001u || (ip >> 24) == 127)
        continue; // loopback
      foundNonLoopback = true;
      uint8_t a = (ip >> 24) & 0xFF;
      uint8_t b = (ip >> 16) & 0xFF;
      bool priv = (a == 10) || (a == 172 && b >= 16 && b <= 31) ||
                  (a == 192 && b == 168) ||
                  (a == 169 && b == 254); // link-local
      if (!priv)
        allPrivate = false;
    }
    // IPv6
    else if (ifa->ifa_addr->sa_family == AF_INET6) {
      auto *sa6 = reinterpret_cast<struct sockaddr_in6 *>(ifa->ifa_addr);
      const uint8_t *b = sa6->sin6_addr.s6_addr;
      // loopback ::1
      bool loopback = true;
      for (int i = 0; i < 15; ++i)
        if (b[i] != 0) {
          loopback = false;
          break;
        }
      if (loopback && b[15] == 1)
        continue;
      foundNonLoopback = true;
      // ULA fc00::/7
      bool ula = (b[0] & 0xFE) == 0xFC;
      // link-local fe80::/10
      bool ll = (b[0] == 0xFE) && ((b[1] & 0xC0) == 0x80);
      if (!ula && !ll)
        allPrivate = false;
    }
  }
  freeifaddrs(ifap);
  // If we only found loopback (no real interfaces), allow — avoids blocking
  // during early boot or container environments.
  return !foundNonLoopback || allPrivate;

#elif defined(_WIN32)
  ULONG bufLen = 15000;
  std::vector<uint8_t> buf(bufLen);
  auto *addrBuf = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
  DWORD flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER;
  if (GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, addrBuf, &bufLen) !=
      ERROR_SUCCESS)
    return true;

  bool foundNonLoopback = false;
  bool allPrivate = true;

  for (auto *a = addrBuf; a != nullptr; a = a->Next) {
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    for (auto *u = a->FirstUnicastAddress; u != nullptr; u = u->Next) {
      auto family = u->Address.lpSockaddr->sa_family;
      if (family == AF_INET) {
        auto *sa = reinterpret_cast<sockaddr_in *>(u->Address.lpSockaddr);
        uint32_t ip = ntohl(sa->sin_addr.s_addr);
        foundNonLoopback = true;
        uint8_t a0 = (ip >> 24) & 0xFF;
        uint8_t b0 = (ip >> 16) & 0xFF;
        bool priv = (a0 == 10) || (a0 == 172 && b0 >= 16 && b0 <= 31) ||
                    (a0 == 192 && b0 == 168) || (a0 == 169 && b0 == 254);
        if (!priv)
          allPrivate = false;
      } else if (family == AF_INET6) {
        auto *sa6 = reinterpret_cast<sockaddr_in6 *>(u->Address.lpSockaddr);
        const uint8_t *bytes = sa6->sin6_addr.s6_bytes;
        bool ula = (bytes[0] & 0xFE) == 0xFC;
        bool ll = (bytes[0] == 0xFE) && ((bytes[1] & 0xC0) == 0x80);
        foundNonLoopback = true;
        if (!ula && !ll)
          allPrivate = false;
      }
    }
  }
  return !foundNonLoopback || allPrivate;

#else
  return true; // Unknown platform — allow
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
  // IPv6: ULA (fc00::/7) and link-local (fe80::/10) are private
  if (host.size() >= 4 && host[2] == ':') {
    unsigned int h0 = 0, h1 = 0;
    if (std::sscanf(host.c_str(), "%x:%x", &h0, &h1) >= 1) {
      uint8_t b0 = (h0 >> 8) & 0xFF;
      uint8_t b1 = h0 & 0xFF;
      if ((b0 & 0xFE) == 0xFC)
        return true; // ULA fc00::/7
      if (b0 == 0xFE && (b1 & 0xC0) == 0x80)
        return true; // link-local fe80::/10
    }
  }
  unsigned int a = 0, b = 0, c = 0, d = 0;
  if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
    if (a == 127)
      return true;
    if (a == 10)
      return true;
    if (a == 192 && b == 168)
      return true;
    if (a == 172 && b >= 16 && b <= 31)
      return true;
    if (a == 169 && b == 254)
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

  // Security: only start the web server if all network interfaces are on
  // private/LAN addresses. Override with HAMCLOCK_FORCE_WEB=1.
  const char *forceEnv = std::getenv("HAMCLOCK_FORCE_WEB");
  bool forced = (forceEnv != nullptr && forceEnv[0] == '1');
  if (!forced && !isHostOnPrivateNetwork()) {
    LOG_W("WebServer", "Public IP detected — web server disabled for security. "
                       "Set HAMCLOCK_FORCE_WEB=1 to override.");
    return;
  }

  running_ = true;
  thread_ = std::thread(&WebServer::run, this);
#endif
}

void WebServer::stop() {
#ifndef __EMSCRIPTEN__
  running_ = false;
  if (svrPtr_) {
    static_cast<httplib::Server *>(svrPtr_)->stop();
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  svrPtr_ = nullptr;
#endif
}

void WebServer::run() {
#ifndef __EMSCRIPTEN__
  httplib::Server svr;
  svrPtr_ = &svr;

  // No authentication required

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
      <button onclick="saveAppearance()" style="margin-top:10px">Save Theme</button>
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
      <button onclick="saveNetwork()">Save</button>
      <div id="net-msg"></div>
    </div>
  </div>

  <div id="cluster" class="panel">
    <div class="card">
      <div class="section-hdr">DX Cluster</div>
      <label><input type="checkbox" id="dx-enabled"> Enable DX Cluster</label>
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

  <script>
    // Tab navigation
    function showTab(name) {
      const ids = ['identity','appearance','status','de-dx','network','cluster','radio','services','brightness'];
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
        const r2 = await fetch('/api/display/status');
        const j = await r2.json();
        document.getElementById('pwr-msg').textContent = 'State: ' + j.power + ' (' + j.method + ')';
      } catch(e) {}
    }

    async function saveAppearance() {
      const theme = document.getElementById('theme').value;
      const params = new URLSearchParams({theme});
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
      } catch(e) {}
    }

    async function saveNetwork() {
      const url = document.getElementById('cors-proxy-url').value.trim();
      const params = new URLSearchParams({cors_proxy_url: url});
      try {
        const r = await fetch('/set_config?' + params);
        const t = await r.text();
        const el = document.getElementById('net-msg');
        el.textContent = t === 'ok' ? 'Saved! Reload WASM app to apply.' : 'Error: ' + t;
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
      } catch(e) {}
    }

    async function saveRadio() {
      const params = new URLSearchParams({
        rig_host: document.getElementById('rig-host').value.trim(),
        rig_port: document.getElementById('rig-port').value,
        rig_auto_tune: document.getElementById('rig-autotune').checked ? '1' : '0',
        rot_host: document.getElementById('rot-host').value.trim(),
        rot_port: document.getElementById('rot-port').value,
        rot_auto_track: document.getElementById('rot-autotrack').checked ? '1' : '0'
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
        document.getElementById('spot-source').value = c.liveSpotSource || 'PSK';
        document.getElementById('spot-age').value = c.liveSpotsMaxAge || 30;
        const bitmask = (c.liveSpotsBands !== undefined) ? c.liveSpotsBands : 0xFFF;
        document.querySelectorAll('#band-chips .chip').forEach((ch, i) => {
          ch.classList.toggle('active', !!(bitmask & (1 << i)));
        });
        document.getElementById('gps-enabled').checked = !!c.gpsEnabled;
        document.getElementById('rss-enabled').checked = !!c.rssEnabled;
        document.getElementById('onta-filter').value = c.ontaFilter || 'all';
      } catch(e) {}
    }

    async function saveServices() {
      let bitmask = 0;
      document.querySelectorAll('#band-chips .chip').forEach((ch, i) => {
        if (ch.classList.contains('active')) bitmask |= (1 << i);
      });
      const params = new URLSearchParams({
        qrz_user: document.getElementById('qrz-user').value.trim(),
        spot_source: document.getElementById('spot-source').value,
        spot_max_age: document.getElementById('spot-age').value,
        spot_bands: bitmask,
        gps_enabled: document.getElementById('gps-enabled').checked ? '1' : '0',
        rss_enabled: document.getElementById('rss-enabled').checked ? '1' : '0',
        onta_filter: document.getElementById('onta-filter').value
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

  svr.Get(
      "/screen", [this](const httplib::Request &req, httplib::Response &res) {
        if (req.has_param("blank")) {
          int blank = StringUtils::safe_stoi(req.get_param_value("blank"));
          SDL_Event event;
          SDL_zero(event);
          event.type = SDL_USEREVENT;
          event.user.code = SDL_USER_EVENT_BLOCK_SLEEP;
          event.user.data1 = blank ? nullptr : (void *)1;
          SDL_PushEvent(&event);

          if (blank) {
            LOG_I("WebServer", "Screen blanking requested via event");
          } else {
            LOG_I("WebServer", "Screen unblanking requested via event");
          }
          res.set_content("ok", "text/plain");
          return;
        }

        if (req.has_param("prevent")) {
          bool prevent = (req.get_param_value("prevent") == "1" ||
                          req.get_param_value("prevent") == "off");
          cfg_->preventSleep = prevent;

          SDL_Event event;
          SDL_zero(event);
          event.type = SDL_USEREVENT;
          event.user.code = SDL_USER_EVENT_BLOCK_SLEEP;
          event.user.data1 = prevent ? (void *)1 : nullptr;
          SDL_PushEvent(&event);

          if (cfgMgr_)
            cfgMgr_->save(*cfg_);
          res.set_content("ok", "text/plain");
          return;
        }

        // Default status
        nlohmann::json j;
        j["prevent_sleep"] = cfg_->preventSleep;
        j["saver_enabled"] = SDL_IsScreenSaverEnabled() == SDL_TRUE;
#ifdef __linux__
        // Check RPi specific display power
        // This is a best effort check, not guaranteed to work on all systems
        FILE *fp = popen("vcgencmd display_power", "r");
        if (fp) {
          char buffer[128];
          if (fgets(buffer, sizeof(buffer), fp) != NULL) {
            std::string output(buffer);
            if (output.find("display_power=0") != std::string::npos) {
              j["display_power"] = false;
            } else if (output.find("display_power=1") != std::string::npos) {
              j["display_power"] = true;
            }
          }
          pclose(fp);
        }
#endif
        res.set_content(j.dump(2), "application/json");
      });

  // ---------------------------------------------------------------------------
  // Propagation Overlay API
  // Provides schemas and proxy support for VOACAP/MUF-RT overlay generation.
  // For on-demand VOACAP overlays, set the OHB_URL environment variable to
  // point to an open-hamclock-backend instance (e.g. http://localhost:8081).
  // ---------------------------------------------------------------------------

  // GET /api/propagation/voacap
  //   Returns overlay schema and proxy URL to open-hamclock-backend.
  //   Parameters: tx_lat, tx_lon, band (80m/40m/.../6m), freq_mhz, hour_utc,
  //               year, month, path (0/1), mode (SSB/CW/FT8/WSPR/AM/RTTY),
  //               watts, overlay_type (muf/reliability/toa)
  svr.Get("/api/propagation/voacap", [this](const httplib::Request &req,
                                            httplib::Response &res) {
    nlohmann::json j;

    // Extract parameters (with defaults from current state)
    double txLat = state_ ? state_->deLocation.lat : 0.0;
    double txLon = state_ ? state_->deLocation.lon : 0.0;
    if (req.has_param("tx_lat"))
      txLat = StringUtils::safe_stod(req.get_param_value("tx_lat"));
    if (req.has_param("tx_lon"))
      txLon = StringUtils::safe_stod(req.get_param_value("tx_lon"));

    // Current UTC time for defaults
    std::time_t nowTs = std::time(nullptr);
    std::tm utcTm{};
#ifdef _WIN32
    gmtime_s(&utcTm, &nowTs);
#else
            gmtime_r(&nowTs, &utcTm);
#endif
    int hourUtc = utcTm.tm_hour;
    int year = utcTm.tm_year + 1900;
    int month = utcTm.tm_mon + 1;
    if (req.has_param("hour_utc"))
      hourUtc = StringUtils::safe_stoi(req.get_param_value("hour_utc"));
    if (req.has_param("year"))
      year = StringUtils::safe_stoi(req.get_param_value("year"));
    if (req.has_param("month"))
      month = StringUtils::safe_stoi(req.get_param_value("month"));

    double freqMhz = 14.074; // default 20m FT8
    std::string band =
        req.has_param("band") ? req.get_param_value("band") : "20m";
    if (band == "80m")
      freqMhz = 3.573;
    else if (band == "40m")
      freqMhz = 7.074;
    else if (band == "30m")
      freqMhz = 10.136;
    else if (band == "20m")
      freqMhz = 14.074;
    else if (band == "17m")
      freqMhz = 18.1;
    else if (band == "15m")
      freqMhz = 21.074;
    else if (band == "12m")
      freqMhz = 24.9;
    else if (band == "10m")
      freqMhz = 28.074;
    else if (band == "6m")
      freqMhz = 50.313;
    if (req.has_param("freq_mhz"))
      freqMhz = StringUtils::safe_stod(req.get_param_value("freq_mhz"));

    std::string mode =
        req.has_param("mode") ? req.get_param_value("mode") : "SSB";
    int watts = req.has_param("watts")
                    ? StringUtils::safe_stoi(req.get_param_value("watts"))
                    : 100;
    int path = req.has_param("path")
                   ? StringUtils::safe_stoi(req.get_param_value("path"))
                   : 0;
    std::string overlayType = req.has_param("overlay_type")
                                  ? req.get_param_value("overlay_type")
                                  : "reliability";

    // Build OHB backend URL from environment or config
    const char *ohbEnv = std::getenv("OHB_URL");
    std::string ohbUrl = ohbEnv ? std::string(ohbEnv) : "";
    if (ohbUrl.empty() && cfg_) {
      // Check config for optional backend URL
      // (future: cfg_->ohbUrl when that field is added to AppConfig)
      ohbUrl = "";
    }

    j["schema_version"] = "1.0";
    j["overlay_type"] = overlayType;
    j["projection"] = "equirectangular";
    j["bounds"] = {
        {"west", -180}, {"east", 180}, {"south", -90}, {"north", 90}};
    j["width"] = 660;
    j["height"] = 330;
    j["request_params"] = {{"tx_lat", txLat},
                           {"tx_lon", txLon},
                           {"freq_mhz", freqMhz},
                           {"band", band},
                           {"hour_utc", hourUtc},
                           {"year", year},
                           {"month", month},
                           {"mode", mode},
                           {"watts", watts},
                           {"path", path},
                           {"overlay_type", overlayType}};

    // Colormaps for each overlay type
    if (overlayType == "muf") {
      j["colormap"] = nlohmann::json::array({
          {{"value", 0}, {"color", "#4000C0"}, {"label", "0 MHz"}},
          {{"value", 4}, {"color", "#0040FF"}, {"label", "4 MHz"}},
          {{"value", 9}, {"color", "#00CCFF"}, {"label", "9 MHz"}},
          {{"value", 15}, {"color", "#80FFFF"}, {"label", "15 MHz"}},
          {{"value", 20}, {"color", "#00FF80"}, {"label", "20 MHz"}},
          {{"value", 27}, {"color", "#FFFF00"}, {"label", "27 MHz"}},
          {{"value", 30}, {"color", "#FF8000"}, {"label", "30 MHz"}},
          {{"value", 35}, {"color", "#FF0000"}, {"label", "35+ MHz"}},
      });
    } else if (overlayType == "toa") {
      j["colormap"] = nlohmann::json::array({
          {{"value", 0}, {"color", "#00FF80"}, {"label", "0 ms"}},
          {{"value", 5}, {"color", "#80FF40"}, {"label", "5 ms"}},
          {{"value", 15}, {"color", "#FFFF00"}, {"label", "15 ms"}},
          {{"value", 25}, {"color", "#FF80C0"}, {"label", "25 ms"}},
          {{"value", 40}, {"color", "#808080"}, {"label", "40 ms"}},
      });
    } else { // reliability
      j["colormap"] = nlohmann::json::array({
          {{"value", 0}, {"color", "#606060"}, {"label", "0%"}},
          {{"value", 21}, {"color", "#CC4080"}, {"label", "21%"}},
          {{"value", 40}, {"color", "#FFFF00"}, {"label", "40%"}},
          {{"value", 60}, {"color", "#80FF40"}, {"label", "60%"}},
          {{"value", 83}, {"color", "#00FF80"}, {"label", "83%"}},
          {{"value", 100}, {"color", "#FFFFFF"}, {"label", "100%"}},
      });
    }

    if (!ohbUrl.empty()) {
      // Backend configured — return proxy URL
      std::string endpoint =
          (overlayType == "muf")   ? "/ham/HamClock/fetchVOACAP-MUF.pl"
          : (overlayType == "toa") ? "/ham/HamClock/fetchVOACAP-TOA.pl"
                                   : "/ham/HamClock/fetchBandConditions.pl";

      char qs[512];
      std::snprintf(qs, sizeof(qs),
                    "TXLAT=%.4f&TXLNG=%.4f&MHZ=%.3f&UTC=%d&YEAR=%d&MONTH=%d&"
                    "PATH=%d&MODE=%s&WATTS=%d&WIDTH=660&HEIGHT=330",
                    txLat, txLon, freqMhz, hourUtc, year, month, path,
                    mode.c_str(), watts);

      j["backend_url"] = ohbUrl;
      j["overlay_endpoint"] = ohbUrl + endpoint + "?" + std::string(qs);
      j["compute_location"] = "backend";
      j["status"] = "backend_configured";
      j["note"] =
          (overlayType != "reliability")
              ? "Note: fetchVOACAP-MUF.pl and fetchVOACAP-TOA.pl are not yet "
                "implemented in open-hamclock-backend. Use "
                "overlay_type=reliability for DE-to-DX band conditions."
              : "Fetch the overlay_endpoint URL to get band conditions data.";
    } else {
      // No backend — return instructions and KC2G fallback
      j["backend_url"] = nullptr;
      j["compute_location"] = "not_configured";
      j["status"] = "backend_not_configured";
      j["setup_instructions"] = {
          {"step1", "Start open-hamclock-backend: cd open-hamclock-backend && "
                    "docker-compose up"},
          {"step2", "Set environment variable OHB_URL=http://localhost:8081"},
          {"step3", "Restart hamclock-next"},
      };
    }

    j["ttl_seconds"] = 1800;
    j["docs"] = "docs/parity.md";

    res.set_content(j.dump(2), "application/json");
  });

  // GET /api/propagation/muf_rt
  //   Returns KC2G real-time MUF map metadata and direct image URL.
  //   No backend required — client fetches image directly from KC2G via CORS
  //   proxy.
  svr.Get("/api/propagation/muf_rt", [](const httplib::Request &,
                                        httplib::Response &res) {
    nlohmann::json j;
    j["schema_version"] = "1.0";
    j["source"] = "kc2g";
    j["description"] = "Near-real-time Maximum Usable Frequency map from KC2G "
                       "ionosonde network";
    j["stations_api"] = "https://prop.kc2g.com/api/stations.json";
    j["projection"] = "equirectangular";
    j["bounds"] = {
        {"west", -180}, {"east", 180}, {"south", -90}, {"north", 90}};
    j["width"] = 660;
    j["height"] = 330;
    j["update_interval_minutes"] = 15;
    j["backend_required"] = false;
    j["colormap_description"] =
        "Blue (0 MHz) → Green (14 MHz) → Yellow (21 MHz) → Red (28+ MHz)";
    j["integration_notes"] = {
        {"step1", "Fetch stations_api data"},
        {"step2", "Use native PropEngine to generate heatmap overlay"},
        {"step3", "Toggle in MapViewMenu; auto-refresh periodic"},
    };
    res.set_content(j.dump(2), "application/json");
  });

  // -------------------------------------------------------------------------
  // Live Web Viewer — MJPEG stream and viewer page
  // -------------------------------------------------------------------------

  // -------------------------------------------------------------------------
  // /live/status — always 200, reports whether interactive input is enabled
  // -------------------------------------------------------------------------
  svr.Get("/live/status",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j;
            j["liveWebEnabled"] = liveWebEnabled_;
            res.set_content(j.dump(), "application/json");
          });

  // -------------------------------------------------------------------------
  // /live/touch — inject pointer click at logical (x,y)
  // GET /live/touch?x=<int>&y=<int>&button=<0|1>
  // -------------------------------------------------------------------------
  svr.Get("/live/touch", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      res.set_content("Live web not enabled", "text/plain");
      return;
    }
    if (!req.has_param("x") || !req.has_param("y")) {
      res.status = 400;
      res.set_content("missing x/y", "text/plain");
      return;
    }
    int lx = StringUtils::safe_stoi(req.get_param_value("x"));
    int ly = StringUtils::safe_stoi(req.get_param_value("y"));
    int button = req.has_param("button")
                     ? StringUtils::safe_stoi(req.get_param_value("button"))
                     : 0;
    bool shift = req.has_param("shift") && req.get_param_value("shift") == "1";

    // Convert logical coords → draw-pixel coords (same pattern as /debug/click)
    int drawW = HamClock::LOGICAL_WIDTH, drawH = HamClock::LOGICAL_HEIGHT;
    SDL_GetRendererOutputSize(renderer_, &drawW, &drawH);
    int px = static_cast<int>(static_cast<float>(lx) / HamClock::LOGICAL_WIDTH *
                              drawW);
    int py = static_cast<int>(static_cast<float>(ly) /
                              HamClock::LOGICAL_HEIGHT * drawH);

    if (shift) {
      SDL_Event sdown{};
      SDL_zero(sdown);
      sdown.type = SDL_KEYDOWN;
      sdown.key.keysym.sym = SDLK_LSHIFT;
      sdown.key.state = SDL_PRESSED;
      SDL_PushEvent(&sdown);
    }

    if (button == 0 && !shift) {
      // Left click without shift: Use AE_TOUCH for robust modal handling
      SDL_Event ev = {};
      ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_TOUCH;
      ev.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(lx));
      ev.user.data2 = reinterpret_cast<void *>(static_cast<intptr_t>(ly));
      SDL_PushEvent(&ev);
    } else {
      // Right click or shifted: use raw mouse events
      SDL_Event down{}, up{};
      SDL_zero(down);
      down.type = SDL_MOUSEBUTTONDOWN;
      down.button.button = (button == 1) ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
      down.button.x = px;
      down.button.y = py;
      down.button.state = SDL_PRESSED;
      down.button.clicks = 1;
      up = down;
      up.type = SDL_MOUSEBUTTONUP;
      up.button.state = SDL_RELEASED;
      up.button.clicks = 1;
      SDL_PushEvent(&down);
      SDL_PushEvent(&up);
    }

    if (shift) {
      SDL_Event sup{};
      SDL_zero(sup);
      sup.type = SDL_KEYUP;
      sup.key.keysym.sym = SDLK_LSHIFT;
      sup.key.state = SDL_RELEASED;
      SDL_PushEvent(&sup);
    }

    res.set_content("ok", "text/plain");
  });

  // /live/wheel — inject mouse wheel (scroll)
  // GET /live/wheel?y=<int>
  // -------------------------------------------------------------------------
  svr.Get("/live/wheel", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      res.set_content("Live web not enabled", "text/plain");
      return;
    }
    if (!req.has_param("y")) {
      res.status = 400;
      res.set_content("missing y", "text/plain");
      return;
    }
    int dy = StringUtils::safe_stoi(req.get_param_value("y"));

    SDL_Event ev = {};
    ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_WHEEL;
    ev.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(dy));
    SDL_PushEvent(&ev);
    res.set_content("ok", "text/plain");
  });

  // -------------------------------------------------------------------------
  // /live/key — inject keyboard input
  // GET /live/key?key=<name>&ctrl=<0|1>&shift=<0|1>
  // -------------------------------------------------------------------------
  svr.Get("/live/key", [this](const httplib::Request &req,
                              httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      res.set_content("Live web not enabled", "text/plain");
      return;
    }
    if (!req.has_param("key")) {
      res.status = 400;
      res.set_content("missing key", "text/plain");
      return;
    }
    std::string k = req.get_param_value("key");
    bool ctrl = req.has_param("ctrl") && req.get_param_value("ctrl") == "1";
    bool shift = req.has_param("shift") && req.get_param_value("shift") == "1";

    SDL_Keycode code = SDLK_UNKNOWN;
    if (k == "enter" || k == "return")
      code = SDLK_RETURN;
    else if (k == "tab")
      code = SDLK_TAB;
    else if (k == "escape" || k == "esc")
      code = SDLK_ESCAPE;
    else if (k == "backspace")
      code = SDLK_BACKSPACE;
    else if (k == "delete" || k == "del")
      code = SDLK_DELETE;
    else if (k == "left")
      code = SDLK_LEFT;
    else if (k == "right")
      code = SDLK_RIGHT;
    else if (k == "up")
      code = SDLK_UP;
    else if (k == "down")
      code = SDLK_DOWN;
    else if (k == "home")
      code = SDLK_HOME;
    else if (k == "end")
      code = SDLK_END;
    else if (k == "space")
      code = SDLK_SPACE;
    else if (k == "f11")
      code = SDLK_F11;

    SDL_Keymod mod = KMOD_NONE;
    if (ctrl)
      mod = static_cast<SDL_Keymod>(mod | KMOD_CTRL);
    if (shift)
      mod = static_cast<SDL_Keymod>(mod | KMOD_SHIFT);

    if (code != SDLK_UNKNOWN) {
      SDL_Event event;
      SDL_zero(event);
      event.type = SDL_KEYDOWN;
      event.key.keysym.sym = code;
      event.key.keysym.mod = mod;
      event.key.state = SDL_PRESSED;
      SDL_PushEvent(&event);
      event.type = SDL_KEYUP;
      event.key.state = SDL_RELEASED;
      SDL_PushEvent(&event);
    } else if (k.size() == 1 && !ctrl) {
      // Printable character — inject as text input
      SDL_Event event;
      SDL_zero(event);
      event.type = SDL_TEXTINPUT;
      event.text.text[0] = k[0];
      SDL_PushEvent(&event);
    } else {
      res.status = 404;
      res.set_content("unknown key", "text/plain");
      return;
    }
    res.set_content("ok", "text/plain");
  });

  // -------------------------------------------------------------------------
  // /live/mouse — inject mouse motion (hover)
  // GET /live/mouse?x=<int>&y=<int>
  // -------------------------------------------------------------------------
  svr.Get("/live/mouse", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!liveWebEnabled_) {
      res.status = 403;
      res.set_content("Live web not enabled", "text/plain");
      return;
    }
    if (!req.has_param("x") || !req.has_param("y")) {
      res.status = 400;
      res.set_content("missing x/y", "text/plain");
      return;
    }
    int lx = StringUtils::safe_stoi(req.get_param_value("x"));
    int ly = StringUtils::safe_stoi(req.get_param_value("y"));

    int drawW = HamClock::LOGICAL_WIDTH, drawH = HamClock::LOGICAL_HEIGHT;
    SDL_GetRendererOutputSize(renderer_, &drawW, &drawH);
    int px = static_cast<int>(static_cast<float>(lx) / HamClock::LOGICAL_WIDTH *
                              drawW);
    int py = static_cast<int>(static_cast<float>(ly) /
                              HamClock::LOGICAL_HEIGHT * drawH);

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_MOUSEMOTION;
    event.motion.x = px;
    event.motion.y = py;
    SDL_PushEvent(&event);
    res.set_content("ok", "text/plain");
  });

  // -------------------------------------------------------------------------
  // /live — interactive canvas-based viewer (always available)
  // -------------------------------------------------------------------------
  svr.Get("/live", [](const httplib::Request &, httplib::Response &res) {
    // Inject LOGICAL dimensions into the JS constants
    char appW[16], appH[16];
    std::snprintf(appW, sizeof(appW), "%d", HamClock::LOGICAL_WIDTH);
    std::snprintf(appH, sizeof(appH), "%d", HamClock::LOGICAL_HEIGHT);

    std::string page = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
  <title>HamClock Live</title>
  <style>
    *{margin:0;padding:0;box-sizing:border-box}
    body{background:#000;overflow:hidden;width:100vw;height:100vh}
    #feed{display:block;width:100vw;height:100vh;object-fit:contain}
    #overlay{position:fixed;top:0;left:0;width:100vw;height:100vh;cursor:crosshair;touch-action:none}
    #badge{position:fixed;top:8px;right:8px;background:#c00;color:#fff;
           font:bold 11px monospace;padding:2px 6px;border-radius:2px;pointer-events:none}
    #iobadge{position:fixed;top:8px;right:54px;background:#333;color:#aaa;
             font:bold 11px monospace;padding:2px 6px;border-radius:2px;pointer-events:none}
  </style>
</head>
<body>
  <img id="feed" src="/stream.mjpeg" crossorigin="anonymous">
  <div id="overlay"></div>
  <div id="badge">LIVE</div>
  <div id="iobadge">...</div>
  <script>
    var APP_W = )html";
    page += appW;
    page += R"html(;
    var APP_H = )html";
    page += appH;
    page += R"html(;
    var THROTTLE_MS = 100;

    var feed = document.getElementById('feed');
    var overlay = document.getElementById('overlay');
    var iobadge = document.getElementById('iobadge');

    var liveEnabled = false;
    var lastMouseMs = 0;

    // Probe live/status
    fetch('/live/status').then(function(r){return r.json();}).then(function(j){
      liveEnabled = !!j.liveWebEnabled;
      iobadge.textContent = liveEnabled ? 'R/W' : 'R/O';
      iobadge.style.background = liveEnabled ? '#003300' : '#330000';
      iobadge.style.color = liveEnabled ? '#0f0' : '#f44';
    }).catch(function(){
      iobadge.textContent = 'R/O';
    });

    // Reconnect on stream error
    feed.onerror = function() {
      setTimeout(function(){ location.reload(); }, 3000);
    };

    function getCoords(clientX, clientY) {
      // The MJPEG image 'feed' might have a different aspect ratio than APP_W:APP_H 
      // if the original app window isn't exactly 800x480.
      var imgW = feed.clientWidth;
      var imgH = feed.clientHeight;
      var natW = feed.naturalWidth || APP_W;
      var natH = feed.naturalHeight || APP_H;

      // Calculate letterboxing/pillarboxing of the image within its client box
      var scale = Math.min(imgW / natW, imgH / natH);
      var viewW = natW * scale;
      var viewH = natH * scale;
      var offX = (imgW - viewW) / 2 + feed.offsetLeft;
      var offY = (imgH - viewH) / 2 + feed.offsetTop;

      // Mouse position relative to the ACTUAL pixels in the JPEG
      var jx = (clientX - offX) / scale;
      var jy = (clientY - offY) / scale;

      // Map JPEG pixels back to logical 800x480 space
      // Note: In HamClock-Next, the content is always rendered at scale 
      // starting at 0,0 in the window (no centering viewport in current main.cpp).
      return {
        x: Math.round(jx * APP_W / natW),
        y: Math.round(jy * APP_H / natH)
      };
    }
    function inBounds(p) { return p.x >= 0 && p.x < APP_W && p.y >= 0 && p.y < APP_H; }

    function sendTouch(ax, ay, btn, shift) {
      if (!liveEnabled) return;
      fetch('/live/touch?x=' + ax + '&y=' + ay + '&button=' + btn + (shift ? '&shift=1' : ''));
    }
    function sendMouse(ax, ay) {
      if (!liveEnabled) return;
      var now = Date.now();
      if (now - lastMouseMs < THROTTLE_MS) return;
      lastMouseMs = now;
      fetch('/live/mouse?x=' + ax + '&y=' + ay);
    }
    function sendKey(key, ctrl, shift) {
      if (!liveEnabled) return;
      fetch('/live/key?key=' + encodeURIComponent(key) + '&ctrl=' + (ctrl?1:0) + '&shift=' + (shift?1:0));
    }
    function sendWheel(delta) {
      if (!liveEnabled) return;
      fetch('/live/wheel?y=' + delta);
    }

    // Pointer events
    overlay.addEventListener('wheel', function(e) {
      if (!liveEnabled) return;
      e.preventDefault();
      // Forward direction: -1 for scroll down, 1 for scroll up
      sendWheel(e.deltaY > 0 ? -1 : 1);
    }, {passive:false});

    overlay.addEventListener('pointerdown', function(e) {
      if (!liveEnabled) return;
      var p = getCoords(e.clientX, e.clientY);
      if (inBounds(p)) sendTouch(p.x, p.y, e.button === 2 ? 1 : 0, e.shiftKey);
    });
    overlay.addEventListener('pointermove', function(e) {
      if (!liveEnabled) return;
      var p = getCoords(e.clientX, e.clientY);
      if (inBounds(p)) sendMouse(p.x, p.y);
    });
    overlay.addEventListener('contextmenu', function(e) { e.preventDefault(); });

    // Keyboard
    document.addEventListener('keydown', function(e) {
      if (!liveEnabled) return;
      var keyMap = {
        'escape':'escape','enter':'enter','tab':'tab','backspace':'backspace',
        'delete':'delete',' ':'space',
        'arrowleft':'left','arrowright':'right','arrowup':'up','arrowdown':'down',
        'home':'home','end':'end','f11':'f11'
      };
      var lk = e.key.toLowerCase();
      var mapped = keyMap[lk];
      if (mapped) {
        e.preventDefault();
        sendKey(mapped, e.ctrlKey, e.shiftKey);
      } else if (e.key.length === 1 && !e.ctrlKey && !e.metaKey) {
        sendKey(e.key, false, e.shiftKey);
      } else if (e.ctrlKey && e.key.length === 1) {
        e.preventDefault();
        sendKey(e.key.toLowerCase(), true, e.shiftKey);
      }
    });

    // Touch (mobile)
    overlay.addEventListener('touchstart', function(e) {
      if (!liveEnabled) return;
      e.preventDefault();
      var t = e.changedTouches[0];
      var p = getCoords(t.clientX, t.clientY);
      if (inBounds(p)) sendTouch(p.x, p.y, 0, e.shiftKey);
    }, {passive:false});
    overlay.addEventListener('touchend', function(e) { e.preventDefault(); }, {passive:false});
    overlay.addEventListener('touchmove', function(e) {
      if (!liveEnabled) return;
      e.preventDefault();
      var t = e.changedTouches[0];
      var p = getCoords(t.clientX, t.clientY);
      if (inBounds(p)) sendMouse(p.x, p.y);
    }, {passive:false});
  </script>
</body>
</html>)html";
    res.set_content(page, "text/html");
  });

  svr.Get("/stream.mjpeg", [this](const httplib::Request &,
                                  httplib::Response &res) {
    if (!frameCapture_) {
      res.status = 503;
      res.set_content("Frame capture not available", "text/plain");
      return;
    }
    uint64_t startSeq = frameCapture_->latestSeq();

    // Subscriber tracking: capturing this shared_ptr in the provider lambda
    // ensures the count is decremented exactly when the connection is
    // closed and the lambda is destroyed.
    auto guard = std::shared_ptr<void>(
        nullptr, [fc = frameCapture_](void *) { fc->removeSubscriber(); });
    frameCapture_->addSubscriber();

    res.set_chunked_content_provider(
        "multipart/x-mixed-replace;boundary=frame",
        [this, lastSeq = startSeq,
         guard](size_t, httplib::DataSink &sink) mutable -> bool {
          if (!sink.is_writable())
            return false;
          uint64_t outSeq = lastSeq;
          auto frame = frameCapture_->waitFrame(lastSeq, 500, outSeq);
          if (frame.empty())
            return sink.is_writable();
          lastSeq = outSeq;
          std::string hdr =
              "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " +
              std::to_string(frame.size()) + "\r\n\r\n";
          if (!sink.write(hdr.data(), hdr.size()))
            return false;
          if (!sink.write(reinterpret_cast<const char *>(frame.data()),
                          frame.size()))
            return false;
          return sink.write("\r\n", 2);
        });
  });

  svr.Get("/api/config",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            nlohmann::json j;
            j["callsign"] = cfg_->callsign;
            j["grid"] = cfg_->grid;
            j["lat"] = cfg_->lat;
            j["lon"] = cfg_->lon;
            j["theme"] = cfg_->theme;
            j["dxClusterEnabled"] = cfg_->dxClusterEnabled;
            j["dxClusterHost"] = cfg_->dxClusterHost;
            j["dxClusterPort"] = cfg_->dxClusterPort;
            j["dxClusterLogin"] = cfg_->dxClusterLogin;
            j["dxClusterUseWSJTX"] = cfg_->dxClusterUseWSJTX;
            j["wsjtxPort"] = cfg_->wsjtxPort;
            j["rigHost"] = cfg_->rigHost;
            j["rigPort"] = cfg_->rigPort;
            j["rigAutoTune"] = cfg_->rigAutoTune;
            j["rotatorHost"] = cfg_->rotatorHost;
            j["rotatorPort"] = cfg_->rotatorPort;
            j["rotatorAutoTrack"] = cfg_->rotatorAutoTrack;
            j["qrzUsername"] = cfg_->qrzUsername;
            // qrzPassword intentionally omitted (write-only)
            std::string src = "PSK";
            if (cfg_->liveSpotSource == LiveSpotSource::RBN)
              src = "RBN";
            else if (cfg_->liveSpotSource == LiveSpotSource::WSPR)
              src = "WSPR";
            j["liveSpotSource"] = src;
            j["liveSpotsMaxAge"] = cfg_->liveSpotsMaxAge;
            j["liveSpotsBands"] = cfg_->liveSpotsBands;
            j["brightness"] = cfg_->brightness;
            j["brightnessSchedule"] = cfg_->brightnessSchedule;
            j["dimHour"] = cfg_->dimHour;
            j["dimMinute"] = cfg_->dimMinute;
            j["brightHour"] = cfg_->brightHour;
            j["brightMinute"] = cfg_->brightMinute;
            j["gpsEnabled"] = cfg_->gpsEnabled;
            j["rssEnabled"] = cfg_->rssEnabled;
            j["ontaFilter"] = cfg_->ontaFilter;
            j["corsProxyUrl"] = cfg_->corsProxyUrl;
            j["hubMode"] = (cfg_->hubMode == HubMode::Master)   ? "master"
                           : (cfg_->hubMode == HubMode::Client) ? "client"
                                                                : "off";
            j["hubIp"] = cfg_->hubIp;
            j["hubPort"] = cfg_->hubPort;
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/api/hub/fetch",
          [this](const httplib::Request &req, httplib::Response &res) {
#ifdef __EMSCRIPTEN__
            res.status = 501;
            res.set_content("not supported on WASM", "text/plain");
            return;
#else
    if (!cfg_ || cfg_->hubMode != HubMode::Master) {
      res.status = 403;
      res.set_content("hub not in master mode", "text/plain");
      return;
    }
    if (!req.has_param("url")) {
      res.status = 400;
      res.set_content("missing url", "text/plain");
      return;
    }
    std::string targetUrl = base64Decode(req.get_param_value("url"));
    LOG_D("WebServer", "Hub master: Received proxy request from {} for {}", req.remote_addr, targetUrl);
    if (targetUrl.size() > 2048) {
      LOG_W("WebServer", "Hub master: URL too long ({} bytes) from {}", targetUrl.size(), req.remote_addr);
      res.status = 400;
      res.set_content("url too long", "text/plain");
      return;
    }
    if (targetUrl.empty() ||
        (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0)) {
      LOG_W("WebServer", "Hub master: Invalid URL protocol from {}", req.remote_addr);
      res.status = 400;
      res.set_content("bad url: must start with http:// or https://", "text/plain");
      return;
    }
    if (isPrivateOrLoopbackUrl(targetUrl)) {
      LOG_W("WebServer", "Hub master: Forbidden private/loopback URL requested by {}: {}", req.remote_addr, targetUrl);
      res.status = 403;
      res.set_content("forbidden: private/loopback addresses not allowed", "text/plain");
      return;
    }
    int maxAge = req.has_param("max_age")
        ? StringUtils::safe_stoi(req.get_param_value("max_age")) : 3600;
    if (!netMgr_) {
      LOG_E("WebServer", "Hub master: NetworkManager null, cannot proxy for {}", req.remote_addr);
      res.status = 503;
      res.set_content("network manager unavailable", "text/plain");
      return;
    }
    std::promise<std::string> prom;
    auto fut = prom.get_future();
    netMgr_->fetchAsync(
        targetUrl, [&prom](std::string body) { prom.set_value(std::move(body)); },
        maxAge);

    if (fut.wait_for(std::chrono::seconds(20)) == std::future_status::timeout) {
      LOG_W("WebServer", "Hub master: Upstream fetch timeout for {}", targetUrl);
      res.status = 504;
      res.set_content("upstream fetch timeout", "text/plain");
      return;
    }

    std::string body = fut.get();
    if (body.empty()) {
      LOG_W("WebServer", "Hub master: Upstream fetch failed for {}", targetUrl);
      res.status = 502;
      res.set_content("upstream fetch failed", "text/plain");
      return;
    }
    LOG_D("WebServer", "Hub master: Returning {} bytes to client {} for {}", body.size(), req.remote_addr, targetUrl);
    res.set_content(body, "application/octet-stream");
#endif
          });

#ifdef ENABLE_DEBUG_API
  svr.Get("/debug/widgets",
          [](const httplib::Request &, httplib::Response &res) {
            auto snapshot = UIRegistry::getInstance().getSnapshot();
            nlohmann::json j = nlohmann::json::object();

            for (const auto &[id, info] : snapshot) {
              nlohmann::json w = nlohmann::json::object();
              w["rect"] = {info.rect.x, info.rect.y, info.rect.w, info.rect.h};
              nlohmann::json actions = nlohmann::json::array();
              for (const auto &action : info.actions) {
                nlohmann::json a = nlohmann::json::object();
                a["name"] = action.name;
                a["rect"] = {action.rect.x, action.rect.y, action.rect.w,
                             action.rect.h};
                actions.push_back(a);
              }
              w["actions"] = actions;
              w["data"] = info.data;
              j[id] = w;
            }

            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/debug/click", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (req.has_param("widget") && req.has_param("action")) {
      std::string wname = req.get_param_value("widget");
      std::string aname = req.get_param_value("action");

      auto snapshot = UIRegistry::getInstance().getSnapshot();
      if (snapshot.count(wname)) {
        const auto &info = snapshot[wname];
        for (const auto &action : info.actions) {
          if (action.name == aname) {
            // Found it! Calculate center in logical coords
            int lx = action.rect.x + action.rect.w / 2;
            int ly = action.rect.y + action.rect.h / 2;

            // Convert logical to "raw" coordinates that set_touch uses.
            float rx = static_cast<float>(lx) /
                       static_cast<float>(HamClock::LOGICAL_WIDTH);
            float ry = static_cast<float>(ly) /
                       static_cast<float>(HamClock::LOGICAL_HEIGHT);

            // Now simulate the click as if it came from /set_touch
            int w = HamClock::LOGICAL_WIDTH, h = HamClock::LOGICAL_HEIGHT;
            SDL_GetRendererOutputSize(renderer_, &w, &h);
            int px = static_cast<int>(rx * w);
            int py = static_cast<int>(ry * h);

            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_MOUSEBUTTONDOWN;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.state = SDL_PRESSED;
            event.button.x = px;
            event.button.y = py;
            SDL_PushEvent(&event);

            SDL_zero(event);
            event.type = SDL_MOUSEBUTTONUP;
            event.button.button = SDL_BUTTON_LEFT;
            event.button.state = SDL_RELEASED;
            event.button.x = px;
            event.button.y = py;
            SDL_PushEvent(&event);

            res.set_content("ok", "text/plain");
            return;
          }
        }
        res.status = 404;
        res.set_content("action not found", "text/plain");
        return;
      }
      res.status = 404;
      res.set_content("widget not found", "text/plain");
      return;
    }
    res.status = 400;
    res.set_content("missing parameters", "text/plain");
  });
#endif // ENABLE_DEBUG_API

  svr.Get("/get_config.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            std::string out;
            out += "Callsign    " + cfg_->callsign + "\n";
            out += "Grid        " + cfg_->grid + "\n";
            out += "Theme       " + cfg_->theme + "\n";
            out += "Lat         " + std::to_string(cfg_->lat) + "\n";
            out += "Lon         " + std::to_string(cfg_->lon) + "\n";
            out += "CorsProxyUrl " + cfg_->corsProxyUrl + "\n";
            res.set_content(out, "text/plain");
          });

  svr.Get("/get_time.txt", [](const httplib::Request &,
                              httplib::Response &res) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    Astronomy::portable_gmtime(&t, &utc);
    char buf[64];
    std::strftime(buf, sizeof(buf), "Clock_UTC %Y-%m-%dT%H:%M:%S Z\n", &utc);
    res.set_content(buf, "text/plain");
  });

  svr.Get(
      "/get_de.txt", [this](const httplib::Request &, httplib::Response &res) {
        if (!state_) {
          res.status = 503;
          return;
        }
        std::string out;
        out += "DE_Callsign " + state_->deCallsign + "\n";
        out += "DE_Grid     " + state_->deGrid + "\n";
        out += "DE_Lat      " + std::to_string(state_->deLocation.lat) + "\n";
        out += "DE_Lon      " + std::to_string(state_->deLocation.lon) + "\n";
        res.set_content(out, "text/plain");
      });

  svr.Get("/get_spacewx.txt", [this](const httplib::Request &,
                                     httplib::Response &res) {
    if (!solar_) {
      res.status = 503;
      return;
    }
    SolarData sd = solar_->get();
    std::string out;
    out += "SFI     " + std::to_string(sd.sfi) + "\n";
    out += "SSN     " + std::to_string(sd.sunspot_number) + "\n";
    { char kbuf[16]; std::snprintf(kbuf, sizeof(kbuf), "%.1f", sd.k_index); out += "Kp      " + std::string(kbuf) + "\n"; }
    out += "Ap      " + std::to_string(sd.a_index) + "\n";
    out += "Bz      " + std::to_string(sd.bz) + "\n";
    out += "Bt      " + std::to_string(sd.bt) + "\n";
    out += "Wind    " + std::to_string((int)sd.solar_wind_speed) + "\n";
    out += "Density " + std::to_string((int)sd.solar_wind_density) + "\n";
    out += "Aurora  " + std::to_string(sd.aurora) + "\n";
    out += "Dst     " + std::to_string(sd.dst) + "\n";
    res.set_content(out, "text/plain");
  });

  svr.Get("/get_sys.txt", [this](const httplib::Request &,
                                 httplib::Response &res) {
    if (!cpu_) {
      res.status = 503;
      return;
    }
    std::string out;
    if (cpu_->isAvailable()) {
      out += "Temp_C      " + std::to_string(cpu_->getTemperature()) + "\n";
      out += "Temp_F      " + std::to_string(cpu_->getTemperatureF()) + "\n";
    }
    auto now = std::chrono::system_clock::now();
    auto uptimeS =
        std::chrono::duration_cast<std::chrono::seconds>(now - startTime_)
            .count();
    out += "Uptime_S    " + std::to_string(uptimeS) + "\n";
    // Load is currently not exposed via CPUMonitor
    res.set_content(out, "text/plain");
  });

  svr.Get("/get_contests.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!contests_) {
              res.status = 503;
              return;
            }
            auto data = contests_->get();
            std::string out;
            for (const auto &c : data.contests) {
              out += c.title + " | " + c.dateDesc + "\n";
            }
            res.set_content(out, "text/plain");
          });

  // Legacy path kept for compatibility; canonical path fixed below
  svr.Get("/get_dxpots.txt", [this](const httplib::Request &,
                                    httplib::Response &res) {
    if (!dxc_) {
      res.status = 503;
      return;
    }
    auto snap = dxc_->snapshot();
    std::string out;
    int count = 0;
    // Iterate in reverse to show newest first
    for (auto it = snap->spots.rbegin(); it != snap->spots.rend() && count < 20;
         ++it, ++count) {
      out += it->txCall + " at " + std::to_string((int)it->freqKhz) + " kHz\n";
    }
    res.set_content(out, "text/plain");
  });

  // Canonical spelling (typo fix: dxpots -> dxspots)
  svr.Get("/get_dxspots.txt", [this](const httplib::Request &,
                                     httplib::Response &res) {
    if (!dxc_) {
      res.status = 503;
      return;
    }
    auto snap = dxc_->snapshot();
    std::string out;
    int count = 0;
    for (auto it = snap->spots.rbegin(); it != snap->spots.rend() && count < 20;
         ++it, ++count) {
      char buf[128];
      std::snprintf(buf, sizeof(buf), "%-12s %8.1f  %-6s  %-6s\n",
                    it->txCall.c_str(), it->freqKhz,
                    it->mode.c_str(), it->rxCall.c_str());
      out += buf;
    }
    res.set_content(out, "text/plain");
  });

  svr.Get("/get_livespots.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!spots_) {
              res.status = 503;
              return;
            }
            auto snap = spots_->snapshot();
            std::string out;
            int count = 0;
            for (auto it = snap->spots.rbegin();
                 it != snap->spots.rend() && count < 20; ++it, ++count) {
              out += it->senderCallsign + " at " +
                     std::to_string((int)it->freqKhz) + " kHz\n";
            }
            res.set_content(out, "text/plain");
          });

  // GET /get_livestats.txt — band counts + max distance per band
  svr.Get("/get_livestats.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!spots_) {
              res.status = 503;
              return;
            }
            auto snap = spots_->snapshot();
            std::string out;
            for (int i = 0; i < kNumBands; ++i) {
              if (snap->bandCounts[i] > 0) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%-5s  %4d\n",
                              kBands[i].name, snap->bandCounts[i]);
                out += buf;
              }
            }
            if (out.empty())
              out = "No live spots\n";
            res.set_content(out, "text/plain");
          });

  // GET /get_satellites.txt — list of all loaded TLE satellite names
  svr.Get("/get_satellites.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!satMgr_) {
              res.status = 503;
              res.set_content("Satellite manager not available\n", "text/plain");
              return;
            }
            auto sats = satMgr_->getSatellites();
            std::string out;
            for (const auto &s : sats)
              out += s.name + "\n";
            if (out.empty())
              out = "No satellites loaded\n";
            res.set_content(out, "text/plain");
          });

  // GET /get_ontheair.txt — POTA/SOTA activators
  svr.Get("/get_ontheair.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!activityStore_) {
              res.status = 503;
              res.set_content("Activity data not available\n", "text/plain");
              return;
            }
            ActivityData data = activityStore_->get();
            std::string out;
            for (const auto &s : data.ontaSpots) {
              char buf[128];
              std::snprintf(buf, sizeof(buf), "%-12s  %-6s  %-6s  %-8s  %.1f\n",
                            s.call.c_str(), s.program.c_str(), s.ref.c_str(),
                            s.mode.c_str(), s.freqKhz);
              out += buf;
            }
            if (out.empty())
              out = "No activators spotted\n";
            res.set_content(out, "text/plain");
          });

  // GET /get_dxpeds.txt — DXpedition list
  svr.Get("/get_dxpeds.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!activityStore_) {
              res.status = 503;
              res.set_content("Activity data not available\n", "text/plain");
              return;
            }
            ActivityData data = activityStore_->get();
            std::string out;
            std::time_t now = std::time(nullptr);
            for (const auto &d : data.dxpeds) {
              std::time_t t0 = std::chrono::system_clock::to_time_t(d.startTime);
              std::time_t t1 = std::chrono::system_clock::to_time_t(d.endTime);
              // Only show ongoing or future
              if (t1 < now)
                continue;
              std::tm tm0{}, tm1{};
              Astronomy::portable_gmtime(&t0, &tm0);
              Astronomy::portable_gmtime(&t1, &tm1);
              char from[16], to[16];
              std::strftime(from, sizeof(from), "%m/%d", &tm0);
              std::strftime(to, sizeof(to), "%m/%d", &tm1);
              char buf[160];
              std::snprintf(buf, sizeof(buf), "%-12s  %-20s  %s - %s\n",
                            d.call.c_str(), d.location.c_str(), from, to);
              out += buf;
            }
            if (out.empty())
              out = "No DXpeditions\n";
            res.set_content(out, "text/plain");
          });

  // GET /get_capture — JPEG screenshot of current frame
  svr.Get("/get_capture",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!frameCapture_) {
              res.status = 503;
              res.set_content("Frame capture not available", "text/plain");
              return;
            }
            uint64_t seq = 0;
            auto jpeg = frameCapture_->waitFrame(0, 200, seq);
            if (jpeg.empty()) {
              res.status = 503;
              res.set_content("No frame available", "text/plain");
              return;
            }
            // Return JPEG as image/bmp alias — callers expecting BMP
            // can use /stream.mjpeg for live; this provides a snapshot
            res.set_content(
                reinterpret_cast<const char *>(jpeg.data()), jpeg.size(),
                "image/jpeg");
          });

  // GET /set_screenlock?lock=on|off — lock/unlock screen interaction
  svr.Get("/set_screenlock",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("lock")) {
              screenLocked_ = (req.get_param_value("lock") == "on");
            } else if (req.has_param("on")) {
              screenLocked_ = true;
            } else if (req.has_param("off")) {
              screenLocked_ = false;
            }
            std::string out = "Screen_locked ";
            out += screenLocked_ ? "1\n" : "0\n";
            res.set_content(out, "text/plain");
          });

  svr.Get("/get_dx.txt", [this](const httplib::Request &,
                                httplib::Response &res) {
    if (!state_) {
      res.status = 503;
      return;
    }
    if (!state_->dxActive) {
      res.set_content("DX not set\n", "text/plain");
      return;
    }
    std::string out;
    out += "DX_Grid     " + state_->dxGrid + "\n";
    out += "DX_Lat      " + std::to_string(state_->dxLocation.lat) + "\n";
    out += "DX_Lon      " + std::to_string(state_->dxLocation.lon) + "\n";
    double dist =
        Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
    double brg =
        Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
    out += "DX_Dist_km  " + std::to_string(static_cast<int>(dist)) + "\n";
    out += "DX_Bearing  " + std::to_string(static_cast<int>(brg)) + "\n";
    res.set_content(out, "text/plain");
  });

  // Programmatic set DE/DX via lat/lon
  svr.Get("/set_mappos", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (!state_) {
      res.status = 503;
      return;
    }
    if (!req.has_param("lat") || !req.has_param("lon")) {
      res.status = 400;
      res.set_content("missing lat/lon", "text/plain");
      return;
    }
    double lat = StringUtils::safe_stod(req.get_param_value("lat"));
    double lon = StringUtils::safe_stod(req.get_param_value("lon"));
    if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
      res.status = 400;
      res.set_content(
          R"({"error":"lat out of range [-90,90] or lon out of range [-180,180]"})",
          "application/json");
      return;
    }
    std::string target = "dx"; // default
    if (req.has_param("target"))
      target = req.get_param_value("target");

    if (target == "de") {
      state_->deLocation = {lat, lon};
      state_->deGrid = Astronomy::latLonToGrid(lat, lon);
    } else {
      state_->dxLocation = {lat, lon};
      state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
      state_->dxActive = true;
    }
    nlohmann::json j;
    j["target"] = target;
    j["lat"] = lat;
    j["lon"] = lon;
    res.set_content(j.dump(), "application/json");
  });

  // --- LEGACY COMPATIBILITY API ---

  // GET /set_displayOnOff?on|off
  svr.Get("/set_displayOnOff",
          [this](const httplib::Request &req, httplib::Response &res) {
            bool on = true;
            if (req.has_param("off"))
              on = false;
            else if (req.has_param("on"))
              on = true;

            if (displayPower_) {
              displayPower_->setPower(on);
            }
            res.set_content("ok", "text/plain");
          });

  // GET /set_newde?lat=...&lon=... OR /set_newde?grid=...
  svr.Get("/set_newde", [this](const httplib::Request &req,
                               httplib::Response &res) {
    if (!state_) {
      res.status = 503;
      return;
    }
    double lat = 0, lon = 0;
    bool found = false;
    if (req.has_param("lat") && req.has_param("lon")) {
      lat = StringUtils::safe_stod(req.get_param_value("lat"));
      lon = StringUtils::safe_stod(req.get_param_value("lon"));
      if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        res.status = 400;
        res.set_content("lat/lon out of range", "text/plain");
        return;
      }
      found = true;
    } else if (req.has_param("grid")) {
      found = Astronomy::gridToLatLon(req.get_param_value("grid"), lat, lon);
    }
    if (found) {
      state_->deLocation = {lat, lon};
      state_->deGrid = Astronomy::latLonToGrid(lat, lon);
      res.set_content("ok", "text/plain");
    } else {
      res.status = 400;
      res.set_content("missing or invalid location", "text/plain");
    }
  });

  // GET /set_newdx?lat=...&lon=... OR /set_newdx?grid=...
  svr.Get("/set_newdx", [this](const httplib::Request &req,
                               httplib::Response &res) {
    if (!state_) {
      res.status = 503;
      return;
    }
    double lat = 0, lon = 0;
    bool found = false;
    if (req.has_param("lat") && req.has_param("lon")) {
      lat = StringUtils::safe_stod(req.get_param_value("lat"));
      lon = StringUtils::safe_stod(req.get_param_value("lon"));
      if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        res.status = 400;
        res.set_content("lat/lon out of range", "text/plain");
        return;
      }
      found = true;
    } else if (req.has_param("grid")) {
      found = Astronomy::gridToLatLon(req.get_param_value("grid"), lat, lon);
    }
    if (found) {
      state_->dxLocation = {lat, lon};
      state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
      state_->dxActive = true;
      res.set_content("ok", "text/plain");
    } else {
      res.status = 400;
      res.set_content("missing or invalid location", "text/plain");
    }
  });

  // GET /set_cluster?host=...&port=...&user=...
  svr.Get("/set_cluster",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("host"))
              cfg_->dxClusterHost = req.get_param_value("host");
            if (req.has_param("port")) {
              int p = StringUtils::safe_stoi(req.get_param_value("port"));
              if (p < 1 || p > 65535) {
                res.status = 400;
                res.set_content("port out of range [1,65535]", "text/plain");
                return;
              }
              cfg_->dxClusterPort = p;
            }
            if (req.has_param("user"))
              cfg_->dxClusterLogin = req.get_param_value("user");

            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok", "text/plain");
          });

  // GET /set_title?call=...
  svr.Get("/set_title",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("call"))
              cfg_->callsign = req.get_param_value("call");

            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok", "text/plain");
          });

  // GET /set_time?ISO=...  or  ?unix=...  or  ?Now  or  ?change=<seconds>
  svr.Get("/set_time",
          [](const httplib::Request &req, httplib::Response &res) {
#if defined(__linux__) || defined(__APPLE__)
            std::time_t newTime = 0;
            bool valid = false;

            if (req.has_param("unix")) {
              newTime = (std::time_t)StringUtils::safe_stod(
                  req.get_param_value("unix"));
              valid = (newTime > 0);
            } else if (req.has_param("ISO")) {
              // Parse ISO8601 UTC: YYYY-MM-DDTHH:MM:SS or YYYY-MM-DDTHH:MM
              std::string iso = req.get_param_value("ISO");
              std::tm tm{};
              int Y = 0, Mo = 0, D = 0, H = 0, Mi = 0, S = 0;
              if (std::sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
                              &Y, &Mo, &D, &H, &Mi, &S) >= 5) {
                tm.tm_year = Y - 1900;
                tm.tm_mon = Mo - 1;
                tm.tm_mday = D;
                tm.tm_hour = H;
                tm.tm_min = Mi;
                tm.tm_sec = S;
                tm.tm_isdst = 0;
                newTime = Astronomy::portable_timegm(&tm);
                valid = (newTime != (std::time_t)-1);
              }
            } else if (req.has_param("Now")) {
              // Trigger NTP sync — just push an event; actual sync in main loop
              SDL_Event event;
              SDL_zero(event);
              event.type = SDL_USEREVENT;
              event.user.code = SDL_USER_EVENT_NTP_SYNC;
              SDL_PushEvent(&event);
              res.set_content("NTP sync requested\n", "text/plain");
              return;
            } else if (req.has_param("change")) {
              double delta =
                  StringUtils::safe_stod(req.get_param_value("change"));
              newTime = std::time(nullptr) + (std::time_t)delta;
              valid = true;
            }

            if (valid) {
              struct timeval tv;
              tv.tv_sec = newTime;
              tv.tv_usec = 0;
              if (settimeofday(&tv, nullptr) == 0) {
                res.set_content("ok\n", "text/plain");
              } else {
                res.status = 500;
                res.set_content("settimeofday failed (need root)\n",
                                "text/plain");
              }
            } else {
              res.status = 400;
              res.set_content(
                  "usage: ?ISO=YYYY-MM-DDTHH:MM, ?unix=N, ?Now, ?change=N\n",
                  "text/plain");
            }
#else
            res.status = 501;
            res.set_content("set_time not supported on this platform\n",
                            "text/plain");
#endif
          });

  // GET /set_alarm?state=armed|off&time=HH:MM&utc=0|1
  svr.Get("/set_alarm",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            if (req.has_param("state")) {
              cfg_->alarmArmed =
                  (req.get_param_value("state") == "armed");
            }
            if (req.has_param("time")) {
              std::string t = req.get_param_value("time");
              int hh = 0, mm = 0;
              if (std::sscanf(t.c_str(), "%d:%d", &hh, &mm) == 2 &&
                  hh >= 0 && hh < 24 && mm >= 0 && mm < 60) {
                cfg_->alarmTimeHH = hh;
                cfg_->alarmTimeMM = mm;
              }
            }
            if (req.has_param("utc")) {
              cfg_->alarmUtc =
                  (req.get_param_value("utc") != "0");
            }
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Alarm %s %02d:%02d %s\n",
                          cfg_->alarmArmed ? "armed" : "off",
                          cfg_->alarmTimeHH, cfg_->alarmTimeMM,
                          cfg_->alarmUtc ? "UTC" : "Local");
            res.set_content(buf, "text/plain");
          });

  // GET /set_once_alarm?state=armed|off&time=YYYY-MM-DDTHH:MM&tz=UTC|DE
  svr.Get("/set_once_alarm",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            if (req.has_param("state")) {
              cfg_->onceAlarmArmed =
                  (req.get_param_value("state") == "armed");
            }
            if (req.has_param("time")) {
              cfg_->onceAlarmTime = req.get_param_value("time");
            }
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            std::string out = "Once_alarm ";
            out += cfg_->onceAlarmArmed ? "armed" : "off";
            out += " " + cfg_->onceAlarmTime + "\n";
            res.set_content(out, "text/plain");
          });

  // GET /set_stopwatch?reset|run|stop|countdown=mins
  svr.Get("/set_stopwatch",
          [](const httplib::Request &req, httplib::Response &res) {
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_USEREVENT;

            if (req.has_param("run")) {
              event.user.code = SDL_USER_EVENT_STOPWATCH_RUN;
            } else if (req.has_param("stop")) {
              event.user.code = SDL_USER_EVENT_STOPWATCH_STOP;
            } else if (req.has_param("reset")) {
              event.user.code = SDL_USER_EVENT_STOPWATCH_RESET;
            } else if (req.has_param("countdown")) {
              int mins =
                  StringUtils::safe_stoi(req.get_param_value("countdown"));
              event.user.code = SDL_USER_EVENT_STOPWATCH_COUNTDOWN;
              event.user.data1 = reinterpret_cast<void *>(
                  static_cast<intptr_t>(mins));
            } else {
              res.status = 400;
              res.set_content(
                  "usage: ?run, ?stop, ?reset, or ?countdown=mins\n",
                  "text/plain");
              return;
            }
            SDL_PushEvent(&event);
            res.set_content("ok\n", "text/plain");
          });

  // GET /get_satellite.txt — current tracked satellite: name, az, el, range, range-rate
  svr.Get("/get_satellite.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!satMgr_) {
              res.status = 503;
              res.set_content("Satellite manager not available\n", "text/plain");
              return;
            }
            std::string name = satMgr_->getTrackedSatellite();
            if (name.empty()) {
              res.set_content("No satellite selected\n", "text/plain");
              return;
            }
            // Find the TLE and compute current observation
            const SatelliteTLE *tle = satMgr_->findByName(name);
            if (!tle) {
              res.set_content("TLE not found for " + name + "\n", "text/plain");
              return;
            }
            Satellite sat(*tle);
            if (state_)
              sat.setObserver(state_->deLocation.lat, state_->deLocation.lon);
            SatObservation obs = sat.predict();
            // Doppler: range-rate to Hz at 145 MHz (2m) and 435 MHz (70cm)
            double freqHz_2m = 145.0e6;
            double freqHz_70cm = 435.0e6;
            double c = 299792.458; // km/s
            double dop2m = -obs.rangeRate / c * freqHz_2m;
            double dop70cm = -obs.rangeRate / c * freqHz_70cm;
            std::string out;
            out += "Sat_name    " + name + "\n";
            char buf[128];
            std::snprintf(buf, sizeof(buf), "Sat_az      %.1f\n", obs.azimuth);
            out += buf;
            std::snprintf(buf, sizeof(buf), "Sat_el      %.1f\n", obs.elevation);
            out += buf;
            std::snprintf(buf, sizeof(buf), "Sat_range   %.1f km\n", obs.range);
            out += buf;
            std::snprintf(buf, sizeof(buf), "Sat_rrate   %.3f km/s\n", obs.rangeRate);
            out += buf;
            std::snprintf(buf, sizeof(buf), "Doppler_2m  %+.0f Hz\n", dop2m);
            out += buf;
            std::snprintf(buf, sizeof(buf), "Doppler_70cm %+.0f Hz\n", dop70cm);
            out += buf;
            res.set_content(out, "text/plain");
          });

  // GET /set_satname?name=... or ?none — select/deselect satellite
  svr.Get("/set_satname",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!satMgr_) {
              res.status = 503;
              res.set_content("Satellite manager not available\n", "text/plain");
              return;
            }
            if (req.has_param("none") ||
                (req.has_param("name") &&
                 req.get_param_value("name") == "none")) {
              satMgr_->trackSatellite("");
              if (cfg_)
                cfg_->selectedSatellite = "";
              if (cfgMgr_ && cfg_)
                cfgMgr_->save(*cfg_);
              res.set_content("ok\n", "text/plain");
              return;
            }
            std::string name;
            if (req.has_param("name")) {
              name = req.get_param_value("name");
            } else if (!req.params.empty()) {
              // Support /set_satname?ISS style (name as bare param key)
              name = req.params.begin()->first;
            }
            if (name.empty()) {
              res.status = 400;
              res.set_content("usage: ?name=SAT or ?none\n", "text/plain");
              return;
            }
            const SatelliteTLE *tle = satMgr_->findByName(name);
            if (!tle) {
              res.status = 404;
              res.set_content("Satellite not found: " + name + "\n", "text/plain");
              return;
            }
            satMgr_->trackSatellite(name);
            if (cfg_)
              cfg_->selectedSatellite = name;
            if (cfgMgr_ && cfg_)
              cfgMgr_->save(*cfg_);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_sattle?name=...&t1=...&t2=... — inject custom TLE
  // t1 and t2 are TLE line 1 and line 2 (URL-encoded)
  svr.Get("/set_sattle",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!satMgr_) {
              res.status = 503;
              res.set_content("Satellite manager not available\n", "text/plain");
              return;
            }
            if (!req.has_param("name") || !req.has_param("t1") ||
                !req.has_param("t2")) {
              res.status = 400;
              res.set_content("usage: ?name=...&t1=...&t2=...\n", "text/plain");
              return;
            }
            SatelliteTLE tle;
            tle.name = req.get_param_value("name");
            tle.line1 = req.get_param_value("t1");
            tle.line2 = req.get_param_value("t2");
            // Basic TLE validation: line 1 starts with "1 ", line 2 with "2 "
            if (tle.line1.size() < 2 || tle.line1[0] != '1' ||
                tle.line2.size() < 2 || tle.line2[0] != '2') {
              res.status = 400;
              res.set_content("invalid TLE format\n", "text/plain");
              return;
            }
            // Parse NORAD ID from line 1 (columns 3-7)
            if (tle.line1.size() >= 7) {
              tle.noradId = std::atoi(tle.line1.substr(2, 5).c_str());
            }
            satMgr_->addCustomTLE(tle);
            res.set_content("ok\n", "text/plain");
          });

  // GET /get_active_pane.txt — return widget currently visible in each pane
  svr.Get("/get_active_pane.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!panes_ || panes_->empty()) {
              res.status = 503;
              res.set_content("Dashboard not ready\n", "text/plain");
              return;
            }
            std::string out;
            for (size_t i = 0; i < panes_->size(); ++i) {
              out += "Pane" + std::to_string(i + 1) + "=";
              out += widgetTypeToString((*panes_)[i]->getActiveType());
              out += '\n';
            }
            res.set_content(out, "text/plain");
          });

  // GET /get_pane.txt — return current widget rotation for all 6 panes
  svr.Get("/get_pane.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            const std::vector<WidgetType> *panes[6] = {
                &cfg_->pane1Rotation, &cfg_->pane2Rotation,
                &cfg_->pane3Rotation, &cfg_->pane4Rotation,
                &cfg_->pane5Rotation, &cfg_->pane6Rotation};
            std::string out;
            for (int i = 0; i < 6; ++i) {
              out += "Pane" + std::to_string(i + 1) + "=";
              for (size_t j = 0; j < panes[i]->size(); ++j) {
                if (j > 0) out += ',';
                out += widgetTypeToString((*panes[i])[j]);
              }
              out += '\n';
            }
            res.set_content(out, "text/plain");
          });

  // GET /set_pane?Pane1=WIDGET&Pane2=WIDGET... — set pane widget rotation lists
  // Widget names are the widgetTypeToString values (e.g. "solar", "dx_cluster")
  svr.Get("/set_pane",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            // Map param names: Pane1..Pane6 -> cfg_ paneNRotation vectors
            // Value is comma-separated list of widget type strings
            struct PaneMap {
              const char *param;
              std::vector<WidgetType> AppConfig::*field;
            };
            PaneMap panes[] = {
                {"Pane1", &AppConfig::pane1Rotation},
                {"Pane2", &AppConfig::pane2Rotation},
                {"Pane3", &AppConfig::pane3Rotation},
                {"Pane4", &AppConfig::pane4Rotation},
                {"Pane5", &AppConfig::pane5Rotation},
                {"Pane6", &AppConfig::pane6Rotation},
            };
            std::string errors;
            for (auto &pm : panes) {
              if (!req.has_param(pm.param))
                continue;
              std::string val = req.get_param_value(pm.param);
              std::vector<WidgetType> rotation;
              // Split by comma
              std::string tok;
              for (char c : val) {
                if (c == ',') {
                  if (!tok.empty()) {
                    // Case-insensitive: lowercase the token
                    std::string lower;
                    lower.reserve(tok.size());
                    for (char ch : tok)
                      lower += (char)std::tolower((unsigned char)ch);
                    WidgetType wt =
                        widgetTypeFromString(lower, WidgetType::SOLAR);
                    rotation.push_back(wt);
                    tok.clear();
                  }
                } else {
                  tok += c;
                }
              }
              if (!tok.empty()) {
                std::string lower;
                lower.reserve(tok.size());
                for (char ch : tok)
                  lower += (char)std::tolower((unsigned char)ch);
                rotation.push_back(
                    widgetTypeFromString(lower, WidgetType::SOLAR));
              }
              if (!rotation.empty())
                (cfg_->*pm.field) = rotation;
            }
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            if (!errors.empty()) {
              res.status = 400;
              res.set_content(errors, "text/plain");
            } else {
              res.set_content("ok\n", "text/plain");
            }
          });

  // GET /set_displayTimes?on=HH:MM&off=HH:MM&idle=mins
  svr.Get("/set_displayTimes",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            if (req.has_param("on")) {
              int hh = 0, mm = 0;
              if (std::sscanf(req.get_param_value("on").c_str(), "%d:%d",
                              &hh, &mm) == 2) {
                cfg_->brightHour = hh;
                cfg_->brightMinute = mm;
                cfg_->brightnessSchedule = true;
              }
            }
            if (req.has_param("off")) {
              int hh = 0, mm = 0;
              if (std::sscanf(req.get_param_value("off").c_str(), "%d:%d",
                              &hh, &mm) == 2) {
                cfg_->dimHour = hh;
                cfg_->dimMinute = mm;
                cfg_->brightnessSchedule = true;
              }
            }
            if (req.has_param("idle")) {
              int mins = StringUtils::safe_stoi(req.get_param_value("idle"));
              cfg_->idleMinutes = std::max(0, mins);
            }
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "DisplayOn %02d:%02d  Off %02d:%02d  Idle %dmin\n",
                          cfg_->brightHour, cfg_->brightMinute,
                          cfg_->dimHour, cfg_->dimMinute,
                          cfg_->idleMinutes);
            res.set_content(buf, "text/plain");
          });

  // GET /set_mapcenter?lng=X — set map center longitude
  svr.Get("/set_mapcenter",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            if (!req.has_param("lng")) {
              res.status = 400;
              res.set_content("usage: ?lng=X\n", "text/plain");
              return;
            }
            double lng = StringUtils::safe_stod(req.get_param_value("lng"));
            if (lng < -180.0 || lng > 180.0) {
              res.status = 400;
              res.set_content("lng out of range [-180,180]\n", "text/plain");
              return;
            }
            cfg_->mapCenterLon = lng;
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_panzoom?pan_x=X&pan_y=Y&zoom=Z — set map pan/zoom
  svr.Get("/set_panzoom",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!cfg_) {
              res.status = 503;
              return;
            }
            if (req.has_param("pan_x"))
              cfg_->mapPanX = StringUtils::safe_stoi(req.get_param_value("pan_x"));
            if (req.has_param("pan_y"))
              cfg_->mapPanY = StringUtils::safe_stoi(req.get_param_value("pan_y"));
            if (req.has_param("zoom")) {
              double z = StringUtils::safe_stod(req.get_param_value("zoom"));
              if (z > 0.0)
                cfg_->mapZoom = z;
            }
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Pan %d,%d Zoom %.2f\n",
                          cfg_->mapPanX, cfg_->mapPanY, cfg_->mapZoom);
            res.set_content(buf, "text/plain");
          });

  // GET /set_rotation?pause|resume|next[&pane=N] or ?widget=NAME[&pane=N]
  // Pause/resume/advance pane widget rotation; optionally target a single pane.
  svr.Get("/set_rotation",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!rotationCmd_) {
              res.status = 503;
              res.set_content("Rotation control not available\n", "text/plain");
              return;
            }
            int pane = -1; // -1 = all panes
            if (req.has_param("pane")) {
              pane = std::stoi(req.get_param_value("pane"));
              if (pane < 0 || pane > 5) {
                res.status = 400;
                res.set_content("pane must be 0-5\n", "text/plain");
                return;
              }
            }
            rotationCmdPane_->store(pane, std::memory_order_relaxed);
            rotationCmdWidget_->store(-1, std::memory_order_relaxed);
            if (req.has_param("pause")) {
              rotationCmd_->store(1, std::memory_order_release);
              res.set_content("ok\n", "text/plain");
            } else if (req.has_param("resume")) {
              rotationCmd_->store(2, std::memory_order_release);
              res.set_content("ok\n", "text/plain");
            } else if (req.has_param("next")) {
              rotationCmd_->store(3, std::memory_order_release);
              res.set_content("ok\n", "text/plain");
            } else if (req.has_param("widget")) {
              static constexpr WidgetType kInvalid = static_cast<WidgetType>(-1);
              WidgetType wt = widgetTypeFromString(
                  req.get_param_value("widget"), kInvalid);
              if (wt == kInvalid) {
                res.status = 400;
                res.set_content("unknown widget name\n", "text/plain");
                return;
              }
              rotationCmdWidget_->store(static_cast<int>(wt),
                                        std::memory_order_relaxed);
              rotationCmd_->store(4, std::memory_order_release);
              res.set_content("ok\n", "text/plain");
            } else {
              res.status = 400;
              res.set_content(
                  "usage: ?pause, ?resume, ?next, or ?widget=NAME (optional: &pane=0-5)\n",
                  "text/plain");
            }
          });

  // GET /set_rotator?state=stop|auto&az=X&el=X — rotator control
  svr.Get("/set_rotator",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!rotatorSvc_) {
              res.status = 503;
              res.set_content("Rotator service not available\n", "text/plain");
              return;
            }
            if (req.has_param("state")) {
              std::string s = req.get_param_value("state");
              if (s == "stop") {
                rotatorSvc_->stopAutoTrack();
                rotatorSvc_->stopRotator();
                res.set_content("ok\n", "text/plain");
                return;
              } else if (s == "auto") {
                rotatorSvc_->setAutoTrackEnabled(true);
                res.set_content("ok\n", "text/plain");
                return;
              }
            }
            if (req.has_param("az") || req.has_param("el")) {
              double az = req.has_param("az")
                              ? StringUtils::safe_stod(req.get_param_value("az"))
                              : 0.0;
              double el = req.has_param("el")
                              ? StringUtils::safe_stod(req.get_param_value("el"))
                              : 0.0;
              if (az < 0.0 || az > 360.0 || el < -90.0 || el > 90.0) {
                res.status = 400;
                res.set_content("az out of [0,360] or el out of [-90,90]\n",
                                "text/plain");
                return;
              }
              bool ok = rotatorSvc_->setPosition(az, el);
              res.set_content(ok ? "ok\n" : "rotator command failed\n",
                              "text/plain");
              return;
            }
            res.status = 400;
            res.set_content(
                "usage: ?state=stop|auto or ?az=X&el=X\n", "text/plain");
          });

  svr.Get(
      "/debug/type", [](const httplib::Request &req, httplib::Response &res) {
        if (req.has_param("text")) {
          std::string text = req.get_param_value("text");
          for (char c : text) {
            SDL_Event event;
            SDL_zero(event);
            event.type = SDL_TEXTINPUT;
            std::snprintf(event.text.text, sizeof(event.text.text), "%c", c);
            SDL_PushEvent(&event);
          }
          res.set_content("ok", "text/plain");
        } else {
          res.status = 400;
          res.set_content("missing 'text' parameter", "text/plain");
        }
      });

  svr.Get("/debug/keypress",
          [](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("key")) {
              std::string k = req.get_param_value("key");
              SDL_Keycode code = SDLK_UNKNOWN;

              if (k == "enter" || k == "return")
                code = SDLK_RETURN;
              else if (k == "tab")
                code = SDLK_TAB;
              else if (k == "escape" || k == "esc")
                code = SDLK_ESCAPE;
              else if (k == "backspace")
                code = SDLK_BACKSPACE;
              else if (k == "delete" || k == "del")
                code = SDLK_DELETE;
              else if (k == "left")
                code = SDLK_LEFT;
              else if (k == "right")
                code = SDLK_RIGHT;
              else if (k == "up")
                code = SDLK_UP;
              else if (k == "down")
                code = SDLK_DOWN;
              else if (k == "home")
                code = SDLK_HOME;
              else if (k == "end")
                code = SDLK_END;
              else if (k == "space")
                code = SDLK_SPACE;
              else if (k == "f11")
                code = SDLK_F11;

              if (code != SDLK_UNKNOWN) {
                SDL_Event event;
                SDL_zero(event);
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = code;
                event.key.state = SDL_PRESSED;
                SDL_PushEvent(&event);

                event.type = SDL_KEYUP;
                event.key.keysym.sym = code;
                event.key.state = SDL_RELEASED;
                SDL_PushEvent(&event);
                res.set_content("ok", "text/plain");
              } else {
                res.status = 404;
                res.set_content("unknown key", "text/plain");
              }
            } else {
              res.status = 400;
              res.set_content("missing 'key' parameter", "text/plain");
            }
          });

  svr.Get("/set_config", [this](const httplib::Request &req,
                                httplib::Response &res) {
    // Server-side length guards for string params (supplements client maxlength)
    auto strParam = [&](const std::string &key, size_t maxLen) -> std::string {
      std::string v = req.get_param_value(key);
      if (v.size() > maxLen) v.resize(maxLen);
      return v;
    };
    if (req.has_param("call"))
      cfg_->callsign = strParam("call", 12);
    if (req.has_param("grid"))
      cfg_->grid = strParam("grid", 8);
    if (req.has_param("theme"))
      cfg_->theme = strParam("theme", 32);
    if (req.has_param("lat")) {
      double v = StringUtils::safe_stod(req.get_param_value("lat"));
      if (v < -90.0 || v > 90.0) {
        res.status = 400;
        res.set_content(R"({"error":"lat out of range [-90,90]"})",
                        "application/json");
        return;
      }
      cfg_->lat = v;
    }
    if (req.has_param("lon")) {
      double v = StringUtils::safe_stod(req.get_param_value("lon"));
      if (v < -180.0 || v > 180.0) {
        res.status = 400;
        res.set_content(R"({"error":"lon out of range [-180,180]"})",
                        "application/json");
        return;
      }
      cfg_->lon = v;
    }
    if (req.has_param("cors_proxy_url"))
      cfg_->corsProxyUrl = req.get_param_value("cors_proxy_url");
    // DX Cluster
    if (req.has_param("dx_enabled"))
      cfg_->dxClusterEnabled = req.get_param_value("dx_enabled") == "1";
    if (req.has_param("dx_host"))
      cfg_->dxClusterHost = strParam("dx_host", 253);
    if (req.has_param("dx_port")) {
      int p = StringUtils::safe_stoi(req.get_param_value("dx_port"));
      if (p < 1 || p > 65535) {
        res.status = 400;
        res.set_content("dx_port out of range", "text/plain");
        return;
      }
      cfg_->dxClusterPort = p;
    }
    if (req.has_param("dx_login"))
      cfg_->dxClusterLogin = req.get_param_value("dx_login");
    if (req.has_param("dx_use_wsjtx"))
      cfg_->dxClusterUseWSJTX = req.get_param_value("dx_use_wsjtx") == "1";
    if (req.has_param("wsjtx_port")) {
      int p = StringUtils::safe_stoi(req.get_param_value("wsjtx_port"));
      if (p < 1 || p > 65535) {
        res.status = 400;
        res.set_content("wsjtx_port out of range", "text/plain");
        return;
      }
      cfg_->wsjtxPort = p;
    }
    // Rig
    if (req.has_param("rig_host"))
      cfg_->rigHost = strParam("rig_host", 253);
    if (req.has_param("rig_port")) {
      int p = StringUtils::safe_stoi(req.get_param_value("rig_port"));
      if (p < 1 || p > 65535) {
        res.status = 400;
        res.set_content("rig_port out of range", "text/plain");
        return;
      }
      cfg_->rigPort = p;
    }
    if (req.has_param("rig_auto_tune"))
      cfg_->rigAutoTune = req.get_param_value("rig_auto_tune") == "1";
    // Rotator
    if (req.has_param("rot_host"))
      cfg_->rotatorHost = strParam("rot_host", 253);
    if (req.has_param("rot_port")) {
      int p = StringUtils::safe_stoi(req.get_param_value("rot_port"));
      if (p < 1 || p > 65535) {
        res.status = 400;
        res.set_content("rot_port out of range", "text/plain");
        return;
      }
      cfg_->rotatorPort = p;
    }
    if (req.has_param("rot_auto_track"))
      cfg_->rotatorAutoTrack = req.get_param_value("rot_auto_track") == "1";
    // QRZ
    if (req.has_param("qrz_user"))
      cfg_->qrzUsername = req.get_param_value("qrz_user");
    if (req.has_param("qrz_pass"))
      cfg_->qrzPassword = req.get_param_value("qrz_pass");
    // Live Spots
    if (req.has_param("spot_source")) {
      const std::string &s = req.get_param_value("spot_source");
      if (s == "RBN")
        cfg_->liveSpotSource = LiveSpotSource::RBN;
      else if (s == "WSPR")
        cfg_->liveSpotSource = LiveSpotSource::WSPR;
      else
        cfg_->liveSpotSource = LiveSpotSource::PSK;
    }
    if (req.has_param("spot_max_age"))
      cfg_->liveSpotsMaxAge =
          StringUtils::safe_stoi(req.get_param_value("spot_max_age"));
    if (req.has_param("spot_bands"))
      cfg_->liveSpotsBands =
          (uint32_t)StringUtils::safe_stoi(req.get_param_value("spot_bands"));
    // Brightness
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
    // Toggles
    if (req.has_param("gps_enabled"))
      cfg_->gpsEnabled = req.get_param_value("gps_enabled") == "1";
    if (req.has_param("rss_enabled"))
      cfg_->rssEnabled = req.get_param_value("rss_enabled") == "1";
    if (req.has_param("onta_filter"))
      cfg_->ontaFilter = req.get_param_value("onta_filter");
    // Hub
    if (req.has_param("hub_mode")) {
      const auto &m = req.get_param_value("hub_mode");
      cfg_->hubMode = (m == "master")   ? HubMode::Master
                      : (m == "client") ? HubMode::Client
                                        : HubMode::Off;
    }
    if (req.has_param("hub_ip"))
      cfg_->hubIp = req.get_param_value("hub_ip");
    if (req.has_param("hub_port"))
      cfg_->hubPort = StringUtils::safe_stoi(req.get_param_value("hub_port"));

    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (netMgr_)
      netMgr_->setHubConfig(cfg_->hubMode, cfg_->hubIp, cfg_->hubPort);
    // Signal the main thread to re-apply the new config to live state.
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  // POST /api/reload — re-applies the current in-memory config to live app
  // state without restarting.  Useful after a remote /set_config call on a
  // framebuffer/headless RPi where you want changes to take effect immediately.
  svr.Post("/api/reload",
           [this](const httplib::Request &, httplib::Response &res) {
             if (reloadFlag_)
               reloadFlag_->store(true, std::memory_order_release);
             res.set_content("{\"ok\":true}", "application/json");
           });

  svr.Get("/debug/watchlist/add",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("call") && watchlist_) {
              watchlist_->add(req.get_param_value("call"));
              res.set_content("ok", "text/plain");
            } else {
              res.status = 400;
              res.set_content("missing call or watchlist store", "text/plain");
            }
          });

  svr.Get("/debug/store/set_solar", [this](const httplib::Request &req,
                                           httplib::Response &res) {
    if (solar_) {
      SolarData data = solar_->get();
      if (req.has_param("sfi"))
        data.sfi = StringUtils::safe_stoi(req.get_param_value("sfi"));
      if (req.has_param("k"))
        data.k_index = StringUtils::safe_stoi(req.get_param_value("k"));
      if (req.has_param("sn"))
        data.sunspot_number = StringUtils::safe_stoi(req.get_param_value("sn"));
      data.valid = true;
      solar_->set(data);
      res.set_content("ok", "text/plain");
    } else {
      res.status = 503;
      res.set_content("solar store not available", "text/plain");
    }
  });

  svr.Get("/debug/performance",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j;
            j["fps"] = state_->fps;
            j["port"] = port_;
            j["running_since"] = SDL_GetTicks() / 1000;
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/debug/logs", [](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j;
    j["status"] = "OK";
    j["info"] = "Logs are written to rotating file (~/.hamclock/hamclock.log) "
                "and stderr (journalctl).";
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/debug/health", [this](const httplib::Request &,
                                  httplib::Response &res) {
    nlohmann::json j;
    for (const auto &[name, status] : state_->services) {
      nlohmann::json s;
      s["ok"] = status.ok;
      s["lastError"] = status.lastError;
      if (status.lastSuccess.time_since_epoch().count() > 0) {
        auto t = std::chrono::system_clock::to_time_t(status.lastSuccess);
        std::tm tm_utc{};
        Astronomy::portable_gmtime(&t, &tm_utc);
        std::stringstream ss;
        ss << std::put_time(&tm_utc, "%Y-%m-%d %H:%M:%S");
        s["lastSuccess"] = ss.str();
      }
      j[name] = s;
    }
    res.set_content(j.dump(2), "application/json");
  });

  // Display Power Control
  svr.Get("/api/display/status",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j;
            if (displayPower_) {
              j["success"] = true;
              j["power"] = displayPower_->getPower() ? "on" : "off";
              j["method"] = displayPower_->getMethodName();
            } else {
              j["success"] = false;
              j["error"] = "DisplayPower module not initialized";
            }
            res.set_content(j.dump(), "application/json");
          });

  svr.Post("/api/display/power",
           [this](const httplib::Request &req, httplib::Response &res) {
             nlohmann::json j;
             bool on = true;

             try {
               auto body = nlohmann::json::parse(req.body);
               if (body.contains("state")) {
                 std::string s = body["state"];
                 on = (s == "on");
               }
             } catch (...) {
               // Fallback to params if JSON parse fails
               if (req.has_param("state")) {
                 on = (req.get_param_value("state") == "on");
               }
             }

             if (displayPower_) {
               bool ok = displayPower_->setPower(on);
               j["success"] = ok;
               j["state"] = on ? "on" : "off";
               j["method"] = displayPower_->getMethodName();
             } else {
               j["success"] = false;
               j["error"] = "DisplayPower module not initialized";
             }
             res.set_content(j.dump(), "application/json");
           });
  // -----------------------------------------------------------------------
  // Utility & headless-control endpoints
  // -----------------------------------------------------------------------

  // GET /get_status.txt — app health: version, uptime, pane states
  svr.Get("/get_status.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            auto now = std::chrono::system_clock::now();
            int uptimeSec = static_cast<int>(
                std::chrono::duration_cast<std::chrono::seconds>(
                    now - startTime_).count());
            std::string out;
            out += "version=" HAMCLOCK_VERSION "\n";
            out += "uptime=" + std::to_string(uptimeSec) + "\n";
            bool paused = panes_ && !panes_->empty() &&
                          (*panes_)[0]->isPaused();
            out += std::string("rotation_paused=") + (paused ? "true" : "false") + "\n";
            if (panes_) {
              for (size_t i = 0; i < panes_->size(); ++i) {
                out += "Pane" + std::to_string(i + 1) + "=";
                out += widgetTypeToString((*panes_)[i]->getActiveType());
                out += '\n';
              }
            }
            res.set_content(out, "text/plain");
          });

  // GET /get_sensors.txt — local BME280 sensor readings (if present)
  svr.Get("/get_sensors.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!weatherStore_) {
              res.status = 503;
              res.set_content("Sensor store not available\n", "text/plain");
              return;
            }
            WeatherData wd = weatherStore_->get();
            if (!wd.valid) {
              res.set_content("valid=false\n", "text/plain");
              return;
            }
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "valid=true\ntempC=%.1f\npressureHpa=%.1f\nhumidity=%d\n",
                          wd.temp, wd.pressure, wd.humidity);
            res.set_content(buf, "text/plain");
          });

  // GET /get_memory.txt — RAM and VRAM usage
  svr.Get("/get_memory.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            int64_t vram = MemoryMonitor::getInstance().getVramEstimated();
            float cpuPct = cpu_ ? cpu_->getCpuPercent() : -1.0f;
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "vram_bytes=%lld\ncpu_percent=%.1f\n",
                          (long long)vram, cpuPct);
            res.set_content(buf, "text/plain");
          });

  // GET /get_config.txt — key runtime config fields
  svr.Get("/get_config.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                          "callsign=%s\ngrid=%s\nlat=%.5f\nlon=%.5f\n"
                          "theme=%s\nmetric=%s\nbrightness=%d\n"
                          "rotation_interval=%d\n",
                          cfg_->callsign.c_str(), cfg_->grid.c_str(),
                          cfg_->lat, cfg_->lon,
                          cfg_->theme.c_str(),
                          cfg_->useMetric ? "true" : "false",
                          cfg_->brightness,
                          cfg_->rotationIntervalS);
            res.set_content(buf, "text/plain");
          });

  // GET /set_callsign?call=W1AW — change operator callsign live
  svr.Get("/set_callsign",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("call") || req.get_param_value("call").empty()) {
              res.status = 400;
              res.set_content("usage: ?call=CALLSIGN\n", "text/plain");
              return;
            }
            cfg_->callsign = req.get_param_value("call");
            cfgMgr_->save(*cfg_);
            reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_location?lat=X&lon=Y[&grid=FN31] — change DE location live
  svr.Get("/set_location",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("lat") || !req.has_param("lon")) {
              res.status = 400;
              res.set_content("usage: ?lat=X&lon=Y[&grid=GRID]\n", "text/plain");
              return;
            }
            double lat = StringUtils::safe_stod(req.get_param_value("lat"));
            double lon = StringUtils::safe_stod(req.get_param_value("lon"));
            if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
              res.status = 400;
              res.set_content("lat out of [-90,90] or lon out of [-180,180]\n",
                              "text/plain");
              return;
            }
            cfg_->lat = lat;
            cfg_->lon = lon;
            if (req.has_param("grid"))
              cfg_->grid = req.get_param_value("grid");
            cfgMgr_->save(*cfg_);
            reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_theme?theme=dark|light|glass|default — switch UI theme live
  svr.Get("/set_theme",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("theme")) {
              res.status = 400;
              res.set_content("usage: ?theme=dark|light|glass|default\n",
                              "text/plain");
              return;
            }
            std::string t = req.get_param_value("theme");
            if (t != "dark" && t != "light" && t != "glass" && t != "default") {
              res.status = 400;
              res.set_content("unknown theme\n", "text/plain");
              return;
            }
            cfg_->theme = t;
            cfgMgr_->save(*cfg_);
            reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_metric?units=metric|imperial — toggle unit system live
  svr.Get("/set_metric",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("units")) {
              res.status = 400;
              res.set_content("usage: ?units=metric|imperial\n", "text/plain");
              return;
            }
            std::string u = req.get_param_value("units");
            if (u == "metric") cfg_->useMetric = true;
            else if (u == "imperial") cfg_->useMetric = false;
            else {
              res.status = 400;
              res.set_content("units must be metric or imperial\n", "text/plain");
              return;
            }
            cfgMgr_->save(*cfg_);
            reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_brightness?pct=0-100 — set display brightness
  svr.Get("/set_brightness",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("pct")) {
              res.status = 400;
              res.set_content("usage: ?pct=0-100\n", "text/plain");
              return;
            }
            int pct = std::stoi(req.get_param_value("pct"));
            if (pct < 0 || pct > 100) {
              res.status = 400;
              res.set_content("pct must be 0-100\n", "text/plain");
              return;
            }
            cfg_->brightness = pct;
            cfgMgr_->save(*cfg_);
            if (brightnessMgr_)
              brightnessMgr_->setBrightness(pct);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_touch?x=X&y=Y — inject a virtual left-click at logical coords
  // Coordinates are in logical space (0-799 x, 0-479 y) regardless of window size.
  svr.Get("/set_touch",
          [](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("x") || !req.has_param("y")) {
              res.status = 400;
              res.set_content("usage: ?x=X&y=Y (logical 0-799, 0-479)\n",
                              "text/plain");
              return;
            }
            int x = std::stoi(req.get_param_value("x"));
            int y = std::stoi(req.get_param_value("y"));
            if (x < 0 || x > 799 || y < 0 || y > 479) {
              res.status = 400;
              res.set_content("x must be 0-799, y must be 0-479\n", "text/plain");
              return;
            }
            SDL_Event ev = {};
            ev.type = AE_BASE_EVENT + HamClock::AE_TOUCH;
            ev.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(x));
            ev.user.data2 = reinterpret_cast<void *>(static_cast<intptr_t>(y));
            SDL_PushEvent(&ev);
            res.set_content("ok\n", "text/plain");
          });

  // GET /set_wheel?y=DELTA — inject a virtual mouse wheel event
  // delta > 0 is scroll up, delta < 0 is scroll down.
  svr.Get("/set_wheel",
          [](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("y")) {
              res.status = 400;
              res.set_content("usage: ?y=DELTA (positive=up, negative=down)\n",
                              "text/plain");
              return;
            }
            int delta = std::stoi(req.get_param_value("y"));
            SDL_Event ev = {};
            ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_WHEEL;
            ev.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(delta));
            SDL_PushEvent(&ev);
            res.set_content("ok\n", "text/plain");
          });

  if (liveWebEnabled_)
    LOG_I("WebServer", "Live web interface ENABLED (interactive /live)");
  LOG_I("WebServer", "Listening on port {}...", port_);
  svr.listen("0.0.0.0", port_);
  svrPtr_ = nullptr;
#endif
}
