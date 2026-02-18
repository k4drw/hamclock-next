#include "WebServer.h"

#include <SDL.h>

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/StringUtils.h"
#include "../core/WatchlistStore.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "../core/Logger.h"

#ifdef ENABLE_DEBUG_API
#include "../core/Astronomy.h"
#include "../core/UIRegistry.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#endif

using namespace HamClock;

WebServer::WebServer(SDL_Renderer *renderer, AppConfig &cfg,
                     HamClockState &state, ConfigManager &cfgMgr,
                     std::shared_ptr<DisplayPower> displayPower,
                     std::atomic<bool> &reloadFlag,
                     std::shared_ptr<WatchlistStore> watchlist,
                     std::shared_ptr<SolarDataStore> solar, int port)
    : renderer_(renderer), cfg_(&cfg), state_(&state), cfgMgr_(&cfgMgr),
      watchlist_(watchlist), solar_(solar), displayPower_(displayPower),
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
    input[type=text], input[type=number] { width: 100%; padding: 6px 8px; background: #222; border: 1px solid var(--dim); color: #eee; font-family: monospace; font-size: 14px; margin-bottom: 10px; }
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

  <div class="tabs">
    <div class="tab active" onclick="showTab('identity')">Identity</div>
    <div class="tab" onclick="showTab('status')">Status</div>
    <div class="tab" onclick="showTab('de-dx')">DE / DX</div>
    <div class="tab" onclick="showTab('network')">Network</div>
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

  <script>
    // Tab navigation
    function showTab(name) {
      document.querySelectorAll('.tab').forEach((t,i) => {
        const ids = ['identity','status','de-dx','network'];
        t.classList.toggle('active', ids[i] === name);
      });
      document.querySelectorAll('.panel').forEach(p => {
        p.classList.toggle('active', p.id === name);
      });
      if (name === 'status') refreshStatus();
      if (name === 'de-dx') refreshDeDx();
      if (name === 'network') loadNetwork();
    }

    // Parse key-value text format ("Key   Value\n")
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
        const r = await fetch('/get_config.txt');
        const kv = parseKV(await r.text());
        document.getElementById('call').value = kv['Callsign'] || '';
        document.getElementById('grid').value = kv['Grid'] || '';
        document.getElementById('lat').value = kv['Lat'] || '';
        document.getElementById('lon').value = kv['Lon'] || '';
      } catch(e) { setMsg('Failed to load config: ' + e, true); }
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
        const r = await fetch('/get_config.txt');
        const kv = parseKV(await r.text());
        document.getElementById('cors-proxy-url').value = kv['CorsProxyUrl'] || '';
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

    // Init
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
  svr.Get("/set_mappos",
          [this](const httplib::Request &req, httplib::Response &res) {
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
            j["grid"] = Astronomy::latLonToGrid(lat, lon);
            res.set_content(j.dump(), "application/json");
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

  svr.Get("/set_config",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("call"))
              cfg_->callsign = req.get_param_value("call");
            if (req.has_param("grid"))
              cfg_->grid = req.get_param_value("grid");
            if (req.has_param("theme"))
              cfg_->theme = req.get_param_value("theme");
            if (req.has_param("lat"))
              cfg_->lat = StringUtils::safe_stod(req.get_param_value("lat"));
            if (req.has_param("lon"))
              cfg_->lon = StringUtils::safe_stod(req.get_param_value("lon"));
            if (req.has_param("cors_proxy_url"))
              cfg_->corsProxyUrl = req.get_param_value("cors_proxy_url");

            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
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
#endif
  LOG_I("WebServer", "Listening on port {}...", port_);
  svr.listen("0.0.0.0", port_);
  svrPtr_ = nullptr;
#endif
}
