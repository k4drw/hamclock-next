#include "WebServer.h"
#include "NetworkManager.h"

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

#include "../core/CPUMonitor.h"
#include "../core/ContestData.h"
#include "../core/DXClusterData.h"
#include "../core/LiveSpotData.h"
#ifdef ENABLE_DEBUG_API
#include "../core/UIRegistry.h"
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
      var scale = Math.min(window.innerWidth / APP_W, window.innerHeight / APP_H);
      var imgW  = APP_W * scale;
      var imgH  = APP_H * scale;
      var offX  = (window.innerWidth  - imgW) / 2;
      var offY  = (window.innerHeight - imgH) / 2;
      return {
        x: Math.round((clientX - offX) / scale),
        y: Math.round((clientY - offY) / scale)
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

    // Pointer events
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
    if (targetUrl.size() > 2048) {
      res.status = 400;
      res.set_content("url too long", "text/plain");
      return;
    }
    if (targetUrl.empty() ||
        (targetUrl.find("http://") != 0 && targetUrl.find("https://") != 0)) {
      res.status = 400;
      res.set_content("bad url: must start with http:// or https://", "text/plain");
      return;
    }
    if (isPrivateOrLoopbackUrl(targetUrl)) {
      res.status = 403;
      res.set_content("forbidden: private/loopback addresses not allowed", "text/plain");
      return;
    }
    int maxAge = req.has_param("max_age")
        ? StringUtils::safe_stoi(req.get_param_value("max_age")) : 3600;
    if (!netMgr_) {
      res.status = 503;
      res.set_content("network manager unavailable", "text/plain");
      return;
    }
    std::promise<std::string> prom;
    auto fut = prom.get_future();
    netMgr_->fetchAsync(targetUrl, [&prom](std::string body) {
      prom.set_value(std::move(body));
    }, maxAge);
    std::string body = fut.get();
    if (body.empty()) {
      res.status = 502;
      res.set_content("upstream fetch failed", "text/plain");
      return;
    }
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
    out += "Kp      " + std::to_string(sd.k_index) + "\n";
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
    if (req.has_param("call"))
      cfg_->callsign = req.get_param_value("call");
    if (req.has_param("grid"))
      cfg_->grid = req.get_param_value("grid");
    if (req.has_param("theme"))
      cfg_->theme = req.get_param_value("theme");
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
      cfg_->dxClusterHost = req.get_param_value("dx_host");
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
      cfg_->rigHost = req.get_param_value("rig_host");
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
      cfg_->rotatorHost = req.get_param_value("rot_host");
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
  if (liveWebEnabled_)
    LOG_I("WebServer", "Live web interface ENABLED (interactive /live)");
  LOG_I("WebServer", "Listening on port {}...", port_);
  svr.listen("0.0.0.0", port_);
  svrPtr_ = nullptr;
#endif
}
