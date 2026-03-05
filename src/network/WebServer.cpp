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
#include "../services/ADIFProvider.h"
#ifdef ENABLE_DEBUG_API
#include "../core/UIRegistry.h"
#endif

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
  case PropOverlayType::None: return "none";
  case PropOverlayType::Muf: return "muf";
  case PropOverlayType::Voacap: return "voacap";
  case PropOverlayType::Reliability: return "reliability";
  case PropOverlayType::Toa: return "toa";
  case PropOverlayType::Heatmap: return "heatmap";
  case PropOverlayType::Drap: return "drap";
  case PropOverlayType::Aurora: return "aurora";
  }
  return "none";
}

[[maybe_unused]] static PropOverlayType propOverlayFromString(const std::string &s) {
  if (s == "muf") return PropOverlayType::Muf;
  if (s == "voacap") return PropOverlayType::Voacap;
  if (s == "reliability") return PropOverlayType::Reliability;
  if (s == "toa") return PropOverlayType::Toa;
  if (s == "heatmap") return PropOverlayType::Heatmap;
  if (s == "drap") return PropOverlayType::Drap;
  if (s == "aurora") return PropOverlayType::Aurora;
  return PropOverlayType::None;
}

[[maybe_unused]] static std::string wxOverlayToString(WeatherOverlayType t) {
  switch (t) {
  case WeatherOverlayType::None: return "none";
  case WeatherOverlayType::Clouds: return "clouds";
  case WeatherOverlayType::WxMb: return "wxmb";
  }
  return "none";
}

[[maybe_unused]] static WeatherOverlayType wxOverlayFromString(const std::string &s) {
  if (s == "clouds") return WeatherOverlayType::Clouds;
  if (s == "wxmb") return WeatherOverlayType::WxMb;
  return WeatherOverlayType::None;
}

static std::string base64Decode(const std::string &in) {
  static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) T[(unsigned char)chars[i]] = i;
  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (T[c] == -1) break;
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
  if (getifaddrs(&ifap) != 0) return true;
  bool foundNonLoopback = false, allPrivate = true;
  for (struct ifaddrs *ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
    auto *sa = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    uint32_t ip = ntohl(sa->sin_addr.s_addr);
    if (ip == 0x7F000001u || (ip >> 24) == 127) continue;
    foundNonLoopback = true;
    uint8_t a = (ip >> 24) & 0xFF, b = (ip >> 16) & 0xFF;
    if (!((a == 10) || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 169 && b == 254))) allPrivate = false;
  }
  freeifaddrs(ifap);
  return !foundNonLoopback || allPrivate;
#else
  return true;
#endif
}

static bool isPrivateOrLoopbackUrl(const std::string &url) {
  size_t pos = url.find("://");
  if (pos == std::string::npos) return true;
  pos += 3;
  size_t end = url.find_first_of("/:?#", pos);
  std::string host = (end == std::string::npos) ? url.substr(pos) : url.substr(pos, end - pos);
  if (!host.empty() && host.front() == '[') host = host.substr(1, host.size() >= 2 ? host.size() - 2 : 0);
  if (host == "localhost" || host == "::1") return true;
  unsigned int a, b, c, d;
  if (std::sscanf(host.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
    if (a == 127 || a == 10 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254)) return true;
  }
  return false;
}

WebServer::WebServer(SDL_Renderer *renderer, AppConfig &cfg, HamClockState &state, ConfigManager &cfgMgr, std::shared_ptr<DisplayPower> displayPower, std::atomic<bool> &reloadFlag, std::shared_ptr<WatchlistStore> watchlist, std::shared_ptr<SolarDataStore> solar, std::shared_ptr<ContestStore> contests, std::shared_ptr<DXClusterDataStore> dxc, std::shared_ptr<LiveSpotDataStore> spots, std::shared_ptr<CPUMonitor> cpu, int port)
    : renderer_(renderer), cfg_(&cfg), state_(&state), cfgMgr_(&cfgMgr), watchlist_(watchlist), solar_(solar), contests_(contests), dxc_(dxc), spots_(spots), cpu_(cpu), displayPower_(displayPower), reloadFlag_(&reloadFlag), port_(port) {}

WebServer::~WebServer() { stop(); }

void WebServer::start() {
#ifndef __EMSCRIPTEN__
  if (running_) return;
  const char *f = std::getenv("HAMCLOCK_FORCE_WEB");
  if (!(f && f[0] == '1') && !isHostOnPrivateNetwork()) return;
  running_ = true; thread_ = std::thread(&WebServer::run, this);
#endif
}

void WebServer::stop() {
#ifndef __EMSCRIPTEN__
  running_ = false;
  if (svrPtr_) static_cast<httplib::Server *>(svrPtr_)->stop();
  if (thread_.joinable()) thread_.join();
  svrPtr_ = nullptr;
#endif
}

void WebServer::run() {
#ifndef __EMSCRIPTEN__
  httplib::Server svr; svrPtr_ = &svr;
  svr.Get("/", [this](const httplib::Request &, httplib::Response &res) {
    std::string html = R"HTML(<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HamClock Control</title><style>
:root{--green:#00e676;--dim:#333;--bg:#111;--card:#1a1a1a}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:#eee;font-family:sans-serif;padding:20px;font-size:1.1rem;line-height:1.4}
h1{color:var(--green);margin-bottom:20px;font-size:1.5rem}
.tabs{display:flex;gap:6px;margin-bottom:20px;flex-wrap:wrap}
.tab{padding:10px 18px;border:1px solid var(--dim);cursor:pointer;color:#aaa;background:#111;font-size:1rem}
.tab.active{border-color:var(--green);color:var(--green);background:#002200}
.panel{display:none}.panel.active{display:block}
.card{background:var(--card);border:1px solid var(--dim);padding:16px;margin-bottom:16px;border-radius:4px}
label{display:block;color:#aaa;font-size:.9rem;margin-bottom:6px;text-transform:uppercase;font-weight:bold}
input,select{width:100%;padding:12px;background:#222;border:1px solid var(--dim);color:#eee;margin-bottom:14px;font-size:1rem;border-radius:4px}
button{padding:12px 24px;background:#003300;border:1px solid var(--green);color:var(--green);cursor:pointer;margin-right:6px;font-size:1rem;border-radius:4px;font-weight:bold}
button:active{background:var(--green);color:#000}
button.danger{border-color:#f44;color:#f44;background:#220000}
.pane-ctrl{display:flex;gap:10px;margin-bottom:10px}
.chip{display:inline-block;padding:6px 12px;border:1px solid #444;margin:3px;font-size:0.9rem;cursor:pointer;border-radius:20px;background:#222}
.chip.active{border-color:var(--green);color:var(--green);background:#003300}
#msg{position:fixed;bottom:20px;right:20px;padding:12px 24px;background:#003300;border:1px solid #0f0;color:#fff;border-radius:4px;box-shadow:0 4px 12px rgba(0,0,0,0.5);display:none;z-index:100}
</style></head><body>
<h1>HamClock Control</h1>
<div class="tabs">
  <div class="tab active" onclick="showTab('identity')">Identity</div>
  <div class="tab" onclick="showTab('map')">Map</div>
  <div class="tab" onclick="showTab('widgets')">Widgets</div>
  <div class="tab" onclick="showTab('presets')">Presets</div>
  <div class="tab" onclick="showTab('hub')">Hub</div>
  <div class="tab" onclick="showTab('status')">Status</div>
</div>
<div id="identity" class="panel active"><div class="card">
  <label>Callsign</label><input type="text" id="call">
  <label>Grid Square</label><input type="text" id="grid">
  <button onclick="saveIdentity()">Save Identity</button>
</div></div>
<div id="map" class="panel"><div class="card">
  <label>Projection</label><select id="map-proj">
    <option value="equirectangular">Equirectangular</option>
    <option value="robinson">Robinson</option>
    <option value="azimuthal">Azimuthal</option>
    <option value="mercator">Mercator</option>
  </select>
  <label>Map Style</label><select id="map-style">
    <option value="nasa">NASA Blue Marble</option>
    <option value="topo">Topography</option>
    <option value="topo_bathy">Topo + Bathymetry</option>
  </select>
  <label>Grid Overlay</label><select id="grid-overlay">
    <option value="none">Off</option>
    <option value="latlon">Lat/Lon</option>
    <option value="maidenhead">Maidenhead</option>
  </select>
  <label>Propagation Overlay</label><select id="prop-overlay">
    <option value="none">None</option>
    <option value="muf">MUF</option>
    <option value="voacap">VOACAP</option>
    <option value="reliability">Reliability</option>
    <option value="toa">TOA</option>
    <option value="heatmap">Heatmap</option>
    <option value="drap">DRAP</option>
    <option value="aurora">Aurora</option>
  </select>
  <label>Weather Overlay</label><select id="wx-overlay">
    <option value="none">None</option>
    <option value="clouds">Clouds</option>
    <option value="wxmb">WX/Pressure</option>
    </select>
    <label><input type="checkbox" id="map-borders"> Country Borders</label>
    <label><input type="checkbox" id="map-night"> Night Lights</label>
    <button onclick="saveMap()">Save Map</button>
    </div></div>
<div id="widgets" class="panel">
  <div class="card">
    <label>Global Pane Controls</label>
    <button onclick="rotateAll()">Next All</button>
    <button onclick="pauseAll(true)">Pause All</button>
    <button onclick="pauseAll(false)">Resume All</button>
  </div>
  <div id="panes-list"></div>
</div>
<div id="presets" class="panel">
  <div class="card">
    <label>New Preset Name</label>
    <div style="display:flex;gap:10px">
      <input type="text" id="new-preset-name" style="margin-bottom:0" placeholder="e.g. Contest Night">
      <button onclick="saveNewPreset()">Save Current as Preset</button>
    </div>
  </div>
  <div id="presets-list"></div>
</div>
<div id="hub" class="panel"><div class="card">
  <label>Hub Mode</label><select id="hub-mode"><option value="Off">Off</option><option value="Master">Master</option><option value="Client">Slave</option></select>
  <label>Master IP (for Slaves)</label><input type="text" id="hub-ip">
  <button onclick="saveHub()">Save Hub</button>
</div></div>
<div id="status" class="panel"><div class="card"><div id="stats-info"></div></div></div>
<div id="msg"></div>
<script>
const tabNames=['identity','map','widgets','presets','hub','status'];
let availWidgets=[];
function setMsg(m){const e=document.getElementById('msg');e.textContent=m;e.style.display='block';setTimeout(()=>e.style.display='none',3000);}
function showTab(n){
  document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('active',tabNames[i]===n));
  document.querySelectorAll('.panel').forEach(p=>p.classList.toggle('active',p.id===n));
  if(n==='widgets')refreshPanes();else if(n==='status')refreshStatus();else if(n==='presets')refreshPresets();else loadConfig();
}
async function loadConfig(){
  const r=await fetch('/api/config');const c=await r.json();
  document.getElementById('call').value=c.callsign;document.getElementById('grid').value=c.grid;
  document.getElementById('map-proj').value=c.projection;
  document.getElementById('map-style').value=c.mapStyle;
  document.getElementById('grid-overlay').value=c.showGrid ? c.gridType : 'none';
  document.getElementById('map-borders').checked = c.showBorders;
  document.getElementById('map-night').checked = c.mapNightLights;
  document.getElementById('prop-overlay').value=c.propOverlay;
  document.getElementById('wx-overlay').value=c.weatherOverlay;
  document.getElementById('hub-mode').value=c.hubMode;document.getElementById('hub-ip').value=c.hubIp;
}
async function saveIdentity(){ sendConfig(new URLSearchParams({call:document.getElementById('call').value,grid:document.getElementById('grid').value})); }
async function saveMap(){ 
  const g = document.getElementById('grid-overlay').value;
  sendConfig(new URLSearchParams({
    projection:document.getElementById('map-proj').value,
    map_style:document.getElementById('map-style').value,
    show_grid: g !== 'none' ? 1 : 0,
    grid_type: g !== 'none' ? g : 'latlon',
    show_borders: document.getElementById('map-borders').checked ? 1 : 0,
    night_lights: document.getElementById('map-night').checked ? 1 : 0,
    prop_overlay:document.getElementById('prop-overlay').value,
    wx_overlay:document.getElementById('wx-overlay').value
  })); 
}
async function saveHub(){ sendConfig(new URLSearchParams({hub_mode:document.getElementById('hub-mode').value,hub_ip:document.getElementById('hub-ip').value})); }
async function sendConfig(p){ await fetch('/set_config?'+p); setMsg('Saved'); }
async function refreshPanes(){
  if(availWidgets.length===0){const r=await fetch('/api/widgets/available');availWidgets=await r.json();}
  const r=await fetch('/api/panes');const p=await r.json();
  const c=document.getElementById('panes-list');c.innerHTML='';
  p.forEach((x,i)=>{
    const d=document.createElement('div');d.className='card';
    let html=`<b>Pane ${i+1}: ${x.current}</b><div class="pane-ctrl"><button onclick="rotate(${i})">Next</button><button onclick="pause(${i})">${x.paused?'Resume':'Pause'}</button></div><div style="margin-top:12px">`;
    availWidgets.forEach(w=>{
      const active=x.rotation.includes(w.display);
      html+=`<span class="chip ${active?'active':''}" onclick="toggleWidget(${i},'${w.id}')">${w.display}</span>`;
    });
    html+=`</div>`;d.innerHTML=html;c.appendChild(d);
  });
}
async function toggleWidget(p,w){ await fetch(`/api/panes/toggle?pane=${p}&widget=${w}`); refreshPanes(); }
async function rotate(i){await fetch('/api/panes/rotate?pane='+i);refreshPanes()}
async function pause(i){await fetch('/api/panes/pause?pane='+i);refreshPanes()}
async function rotateAll(){await fetch('/api/panes/rotate_all');refreshPanes()}
async function pauseAll(p){await fetch('/api/panes/pause_all?paused='+(p?1:0));refreshPanes()}
async function refreshPresets(){
  const r=await fetch('/api/presets');const p=await r.json();
  const c=document.getElementById('presets-list');c.innerHTML='<label>Saved Presets</label>';
  p.forEach((x,i)=>{
    const d=document.createElement('div');d.className='card';
    d.innerHTML=`<div style="display:flex;justify-content:space-between;align-items:center">
      <span style="font-weight:bold">${x}</span>
      <div>
        <button onclick="applyPreset(${i})">Apply</button>
        <button class="danger" onclick="deletePreset(${i})">Delete</button>
      </div>
    </div>`;
    c.appendChild(d);
  });
}
async function saveNewPreset(){
  const name=document.getElementById('new-preset-name').value.trim();
  if(!name){setMsg('Please enter a name');return;}
  await fetch('/api/presets/save?name='+encodeURIComponent(name));
  document.getElementById('new-preset-name').value='';
  setMsg('Preset Saved'); refreshPresets();
}
async function applyPreset(i){await fetch('/api/presets/apply?index='+i);setMsg('Preset Applied');}
async function deletePreset(i){if(confirm('Delete this preset?')){await fetch('/api/presets/delete?index='+i);refreshPresets();}}
async function refreshStatus(){
  const r=await fetch('/api/display/status');const j=await r.json();
  document.getElementById('stats-info').innerHTML=`Uptime: <b>${j.uptime}</b><br>FPS: <b>${j.fps}</b>`;
}
loadConfig();
</script></body></html>)HTML";
    res.set_content(html, "text/html");
  });

  svr.Get("/api/widgets/available", [](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j = nlohmann::json::array();
    static const WidgetType all[] = {
      WidgetType::SOLAR, WidgetType::DX_CLUSTER, WidgetType::LIVE_SPOTS, WidgetType::BAND_CONDITIONS,
      WidgetType::CONTESTS, WidgetType::ON_THE_AIR, WidgetType::GIMBAL, WidgetType::MOON,
      WidgetType::CLOCK_AUX, WidgetType::DX_PEDITIONS, WidgetType::DE_WEATHER, WidgetType::DX_WEATHER,
      WidgetType::NCDXF, WidgetType::SDO, WidgetType::HISTORY_FLUX, WidgetType::HISTORY_KP,
      WidgetType::HISTORY_SSN, WidgetType::DRAP, WidgetType::AURORA, WidgetType::AURORA_GRAPH,
      WidgetType::ADIF, WidgetType::EME_TOOL, WidgetType::SYS_INFO, WidgetType::ASTEROID,
      WidgetType::ALERTS, WidgetType::FORECAST, WidgetType::HURRICANE, WidgetType::MARINE,
      WidgetType::GREYLINE_DX, WidgetType::METEOR, WidgetType::IONOSONDE, WidgetType::SOLAR_STORM
    };
    for (auto t : all) {
      nlohmann::json w; w["id"] = widgetTypeToString(t); w["display"] = widgetTypeDisplayName(t);
      j.push_back(w);
    }
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/api/panes", [this](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j = nlohmann::json::array();
    if (panes_) {
      for (const auto &p : *panes_) {
        nlohmann::json pj; pj["current"] = widgetTypeDisplayName(p->getActiveType());
        pj["paused"] = p->isPaused(); nlohmann::json rot = nlohmann::json::array();
        for (auto t : p->getRotation()) rot.push_back(widgetTypeDisplayName(t));
        pj["rotation"] = rot; j.push_back(pj);
      }
    }
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/api/panes/rotate", [this](const httplib::Request &req, httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("pane"));
    if (panes_ && idx >= 0 && idx < (int)panes_->size()) { (*panes_)[idx]->forceAdvance(); res.set_content("ok", "text/plain"); }
    else res.status = 404;
  });

  svr.Get("/api/panes/rotate_all", [this](const httplib::Request &, httplib::Response &res) {
    if (panes_) { for (auto &p : *panes_) p->forceAdvance(); res.set_content("ok", "text/plain"); }
    else res.status = 503;
  });

  svr.Get("/api/panes/pause", [this](const httplib::Request &req, httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("pane"));
    if (panes_ && idx >= 0 && idx < (int)panes_->size()) { (*panes_)[idx]->setPaused(!(*panes_)[idx]->isPaused()); res.set_content("ok", "text/plain"); }
    else res.status = 404;
  });

  svr.Get("/api/panes/pause_all", [this](const httplib::Request &req, httplib::Response &res) {
    bool p = req.get_param_value("paused") == "1";
    if (panes_) { for (auto &pane : *panes_) pane->setPaused(p); res.set_content("ok", "text/plain"); }
    else res.status = 503;
  });

  svr.Get("/api/panes/toggle", [this](const httplib::Request &req, httplib::Response &res) {
    int pIdx = StringUtils::safe_stoi(req.get_param_value("pane"));
    std::string wId = req.get_param_value("widget");
    if (panes_ && pIdx >= 0 && pIdx < (int)panes_->size()) {
      auto &pane = (*panes_)[pIdx];
      std::vector<WidgetType> rot = pane->getRotation();
      WidgetType target = widgetTypeFromString(wId, WidgetType::SOLAR);
      auto it = std::find(rot.begin(), rot.end(), target);
      if (it != rot.end()) rot.erase(it); else rot.push_back(target);
      if (rot.empty()) rot.push_back(WidgetType::SOLAR);
      pane->setRotation(rot, cfg_->rotationIntervalS, cfg_->syncRotation);
      if (pIdx==0) cfg_->pane1Rotation=rot; else if (pIdx==1) cfg_->pane2Rotation=rot;
      else if (pIdx==2) cfg_->pane3Rotation=rot; else if (pIdx==3) cfg_->pane4Rotation=rot;
      else if (pIdx==4) cfg_->pane5Rotation=rot; else if (pIdx==5) cfg_->pane6Rotation=rot;
      if (cfgMgr_) cfgMgr_->save(*cfg_);
      res.set_content("ok", "text/plain");
    } else res.status = 404;
  });

  svr.Get("/api/presets", [this](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j = nlohmann::json::array();
    if (cfg_) { for (const auto &p : cfg_->presets) j.push_back(p.name); }
    res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/api/presets/apply", [this](const httplib::Request &req, httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("index"));
    if (cfg_ && idx >= 0 && idx < (int)cfg_->presets.size()) {
      cfgMgr_->applyPreset(*cfg_, idx); cfgMgr_->save(*cfg_);
      if (reloadFlag_) reloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else res.status = 404;
  });

  svr.Get("/api/presets/save", [this](const httplib::Request &req, httplib::Response &res) {
    if (!req.has_param("name")) { res.status = 400; return; }
    std::string name = req.get_param_value("name");
    if (cfg_) {
      cfgMgr_->savePreset(*cfg_, name); cfgMgr_->save(*cfg_);
      res.set_content("ok", "text/plain");
    } else res.status = 503;
  });

  svr.Get("/api/presets/delete", [this](const httplib::Request &req, httplib::Response &res) {
    int idx = StringUtils::safe_stoi(req.get_param_value("index"));
    if (cfg_ && idx >= 0 && idx < (int)cfg_->presets.size()) {
      cfgMgr_->deletePreset(*cfg_, idx); cfgMgr_->save(*cfg_);
      res.set_content("ok", "text/plain");
    } else res.status = 404;
  });

  svr.Get("/api/config", [this](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j; j["callsign"] = cfg_->callsign; j["grid"] = cfg_->grid;
    j["theme"] = cfg_->theme; j["projection"] = cfg_->projection;
    j["mapStyle"] = cfg_->mapStyle;
    j["showGrid"] = cfg_->showGrid;
    j["gridType"] = cfg_->gridType;
    j["showBorders"] = cfg_->showBorders;
    j["mapNightLights"] = cfg_->mapNightLights;
    j["propOverlay"] = propOverlayToString(cfg_->propOverlay);
    j["weatherOverlay"] = wxOverlayToString(cfg_->weatherOverlay);
    j["hubMode"] = (cfg_->hubMode == HubMode::Master) ? "Master" : (cfg_->hubMode == HubMode::Client ? "Client" : "Off");
    j["hubIp"] = cfg_->hubIp; res.set_content(j.dump(2), "application/json");
  });

  svr.Get("/set_config", [this](const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("call")) cfg_->callsign = req.get_param_value("call");
    if (req.has_param("grid")) cfg_->grid = req.get_param_value("grid");
    if (req.has_param("projection")) cfg_->projection = req.get_param_value("projection");
    if (req.has_param("map_style")) {
      std::string s = req.get_param_value("map_style");
      if (!s.empty()) cfg_->mapStyle = s;
    }
    if (req.has_param("show_grid")) cfg_->showGrid = req.get_param_value("show_grid") == "1";
    if (req.has_param("show_borders")) cfg_->showBorders = req.get_param_value("show_borders") == "1";
    if (req.has_param("night_lights")) cfg_->mapNightLights = req.get_param_value("night_lights") == "1";
    if (req.has_param("grid_type")) cfg_->gridType = req.get_param_value("grid_type");
    if (req.has_param("prop_overlay")) cfg_->propOverlay = propOverlayFromString(req.get_param_value("prop_overlay"));
    if (req.has_param("wx_overlay")) cfg_->weatherOverlay = wxOverlayFromString(req.get_param_value("wx_overlay"));
    if (req.has_param("hub_mode")) {
      std::string m = req.get_param_value("hub_mode");
      if (m == "Master") cfg_->hubMode = HubMode::Master;
      else if (m == "Client") cfg_->hubMode = HubMode::Client;
      else cfg_->hubMode = HubMode::Off;
    }
    if (req.has_param("hub_ip")) cfg_->hubIp = req.get_param_value("hub_ip");
    if (cfgMgr_) cfgMgr_->save(*cfg_);
    if (reloadFlag_) reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_rss", [this](const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("enabled")) {
      cfg_->rssEnabled = req.get_param_value("enabled") == "1";
      if (cfgMgr_) cfgMgr_->save(*cfg_);
      if (reloadFlag_) reloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else {
      res.status = 400;
    }
  });

  svr.Get("/set_mapcolor", [this](const httplib::Request &req, httplib::Response &res) {
    if (req.has_param("key") && req.has_param("color")) {
      std::string key = req.get_param_value("key");
      std::string color = req.get_param_value("color");
      cfg_->colorOverrides[key] = hexToColor(color);
      if (cfgMgr_) cfgMgr_->save(*cfg_);
      if (reloadFlag_) reloadFlag_->store(true, std::memory_order_release);
      res.set_content("ok", "text/plain");
    } else {
      res.status = 400;
    }
  });

  svr.Post("/set_adif", [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get("/api/display/status", [this](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j; j["fps"] = state_ ? std::to_string(state_->fps).substr(0,4) : "0";
    j["uptime"] = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - startTime_).count()) + "s";
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/api/hub/fetch", [this](const httplib::Request &req, httplib::Response &res) {
    if (!cfg_ || cfg_->hubMode != HubMode::Master) { res.status = 403; return; }
    std::string targetUrl = base64Decode(req.get_param_value("url"));
    if (isPrivateOrLoopbackUrl(targetUrl)) { res.status = 403; return; }
    std::promise<std::string> prom; auto fut = prom.get_future();
    netMgr_->fetchAsync(targetUrl, [&prom](std::string b) { prom.set_value(std::move(b)); }, 3600);
    if (fut.wait_for(std::chrono::seconds(20)) == std::future_status::timeout) { res.status = 504; return; }
    std::string body = fut.get();
    if (body.empty()) { res.status = 502; return; }
    res.set_content(body, "application/octet-stream");
  });

  svr.listen("0.0.0.0", port_);
#endif
}
