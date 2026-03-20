#ifndef CPPHTTPLIB_THREAD_POOL_COUNT
#define CPPHTTPLIB_THREAD_POOL_COUNT 2
#endif
#include "WebServer.h"
#include "../core/Logger.h"
#include "../ui/PaneContainer.h"
#include "../core/CalendarData.h"
#include "../ui/StopwatchPanel.h"
#include "../ui/TimePanel.h"
#include "NetworkManager.h"
#include <SDL.h>
#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include "../core/SolarData.h"
#include "../core/StringUtils.h"
#include "../core/WatchlistStore.h"
#include "../core/WeatherData.h"
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
#include "../core/Theme.h"
#include "../core/DXClusterData.h"
#include "../core/LiveSpotData.h"
#include "../core/SatelliteManager.h"
#include "../network/FrameCapture.h"
#include "../services/ADIFProvider.h"
#ifdef ENABLE_DEBUG_API
#include "../core/UIRegistry.h"
#endif
#include <iomanip>
#include <algorithm>
#include <filesystem>

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

[[maybe_unused]] static PropOverlayType propOverlayFromString(
    const std::string &s) {
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
    case WeatherOverlayType::WxMb:
      return "wxmb";
    case WeatherOverlayType::CloudsGrib:
      return "clouds_grib";
  }
  return "none";
}

[[maybe_unused]] static WeatherOverlayType wxOverlayFromString(
    const std::string &s) {
  if (s == "clouds_grib" || s == "clouds")
    return WeatherOverlayType::CloudsGrib;
  if (s == "wxmb")
    return WeatherOverlayType::WxMb;
  return WeatherOverlayType::None;
}

static std::string base64Decode(const std::string &in) {
  // URL-safe base64 (RFC 4648 §5): '-' and '_' instead of '+' and '/'
  static const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  std::vector<int> T(256, -1);
  for (int i = 0; i < 64; i++) T[(unsigned char)chars[i]] = i;
  std::string out;
  int val = 0, valb = -8;
  for (unsigned char c : in) {
    if (T[c] == -1)
      continue;  // skip padding ('=') and any stray characters
    val = (val << 6) + T[c];
    valb += 6;
    if (valb >= 0) {
      out.push_back(char((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return out;
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

void WebServer::registerRoutes(httplib::Server &svr) {
  svr.Get("/api/widgets/available",
          [](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            auto all = getAllBaseWidgetTypes();
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
            std::lock_guard<std::mutex> lk(dataMutex_);
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
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (panes_ && idx >= 0 && idx < (int)panes_->size()) {
              (*panes_)[idx]->forceAdvance();
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/panes/rotate_all",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (panes_) {
              for (auto &p : *panes_) p->forceAdvance();
              res.set_content("ok", "text/plain");
            } else
              res.status = 503;
          });

  svr.Get("/api/panes/pause",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("pane"));
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (panes_ && idx >= 0 && idx < (int)panes_->size()) {
              (*panes_)[idx]->setPaused(!(*panes_)[idx]->isPaused());
              res.set_content("ok", "text/plain");
            } else
              res.status = 404;
          });

  svr.Get("/api/panes/pause_all",
          [this](const httplib::Request &req, httplib::Response &res) {
            bool p = req.get_param_value("paused") == "1";
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (panes_) {
              for (auto &pane : *panes_) pane->setPaused(p);
              res.set_content("ok", "text/plain");
            } else
              res.status = 503;
          });

  svr.Get("/get_pane_rect",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("pane")) - 1;
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (!panes_ || idx < 0 || idx >= (int)panes_->size()) {
              res.status = 404;
              return;
            }
            SDL_Rect r = (*panes_)[idx]->getRect();
            int outW = 0, outH = 0;
            int logW = 0, logH = 0;
            if (renderer_) {
              SDL_GetRendererOutputSize(renderer_, &outW, &outH);
              SDL_RenderGetLogicalSize(renderer_, &logW, &logH);
            }
            // If no logical size is set, logical is HamClock's native 800x480
            if (logW == 0)
              logW = 800;
            if (logH == 0)
              logH = 480;
            nlohmann::json j;
            j["x"] = r.x;
            j["y"] = r.y;
            j["w"] = r.w;
            j["h"] = r.h;
            j["renderer_w"] = outW;
            j["renderer_h"] = outH;
            j["logical_w"] = logW;
            j["logical_h"] = logH;
            res.set_content(j.dump(), "application/json");
          });

  svr.Get("/api/panes/expand",
          [this](const httplib::Request &req, httplib::Response &res) {
            int idx = StringUtils::safe_stoi(req.get_param_value("pane")) - 1;
            if (idx < 0 || idx > 5) {
              res.status = 400;
              return;
            }
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (paneExpandCmd_)
              paneExpandCmd_->store(idx, std::memory_order_release);
            res.set_content("ok", "text/plain");
          });

  svr.Get("/api/panes/collapse",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (paneExpandCmd_)
              paneExpandCmd_->store(-2, std::memory_order_release);
            res.set_content("ok", "text/plain");
          });

  svr.Get("/get_timepanel_rect",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (!timePanel_) {
              res.status = 503;
              return;
            }
            SDL_Rect r = timePanel_->getRect();
            int outW = 0, outH = 0;
            int logW = 0, logH = 0;
            if (renderer_) {
              SDL_GetRendererOutputSize(renderer_, &outW, &outH);
              SDL_RenderGetLogicalSize(renderer_, &logW, &logH);
            }
            if (logW == 0)
              logW = 800;
            if (logH == 0)
              logH = 480;
            nlohmann::json j;
            j["x"] = r.x;
            j["y"] = r.y;
            j["w"] = r.w;
            j["h"] = r.h;
            j["renderer_w"] = outW;
            j["renderer_h"] = outH;
            j["logical_w"] = logW;
            j["logical_h"] = logH;
            res.set_content(j.dump(), "application/json");
          });

  svr.Get("/api/panes/toggle", [this](const httplib::Request &req,
                                      httplib::Response &res) {
    int pIdx = StringUtils::safe_stoi(req.get_param_value("pane"));
    std::string wId = req.get_param_value("widget");
    std::lock_guard<std::mutex> lk(dataMutex_);
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
              for (const auto &p : cfg_->presets) j.push_back(p.name);
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
    j["showBeacons"] = cfg_->showBeacons;
    j["showSatTrack"] = cfg_->showSatTrack;
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
    j["auxClockTzOffset"] = cfg_->auxClockTzOffset;
    j["auxClockTzLabel"] = cfg_->auxClockTzLabel;
    j["auxClockStarMode"] = cfg_->auxClockStarMode;
    j["lat"] = cfg_->lat;
    j["lon"] = cfg_->lon;
    j["audioMuted"] = cfg_->audioMuted;
    j["fontPath"] = cfg_->fontPath;
    j["version"] = HAMCLOCK_VERSION;
    j["arch"] = HAMCLOCK_ARCH;
    j["installType"] = HAMCLOCK_INSTALL_TYPE;

    nlohmann::json panes = nlohmann::json::array();
    auto addPane = [&](const std::vector<WidgetType> &rot) {
      nlohmann::json pj;
      nlohmann::json r = nlohmann::json::array();
      for (auto t : rot) r.push_back(widgetTypeDisplayName(t));
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
    j["panelMode"] = cfg_->panelMode;
    j["selectedSatellite"] = cfg_->selectedSatellite;
    j["displayPowerMethod"] = cfg_->displayPowerMethod;
    if (displayPower_) {
      nlohmann::json dpm = nlohmann::json::array();
      for (const auto &m : displayPower_->getMethods()) {
        dpm.push_back(DisplayPower::methodToString(m));
      }
      j["displayPowerMethods"] = dpm;
    }

    res.set_content(j.dump(2), "application/json");
  });

  // Alias: /get_config.json → /api/config (consistent with *.txt naming
  // convention)
  svr.Get("/get_config.json",
          [](const httplib::Request &, httplib::Response &res) {
            res.set_redirect("/api/config", 302);
          });

  svr.Get("/set_config", [this](const httplib::Request &req,
                                httplib::Response &res) {
    if (req.has_param("call"))
      cfg_->callsign = req.get_param_value("call");
    if (req.has_param("theme"))
      cfg_->theme = req.get_param_value("theme");
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
    if (req.has_param("show_beacons"))
      cfg_->showBeacons = req.get_param_value("show_beacons") == "1";
    if (req.has_param("show_sattrack"))
      cfg_->showSatTrack = req.get_param_value("show_sattrack") == "1";
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
      cfg_->liveSpotsUseCall =
          req.get_param_value("live_spots_use_call") == "1";
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

    if (req.has_param("aux_tz_offset"))
      cfg_->auxClockTzOffset =
          StringUtils::safe_stoi(req.get_param_value("aux_tz_offset"));
    if (req.has_param("aux_tz_label"))
      cfg_->auxClockTzLabel = req.get_param_value("aux_tz_label");
    if (req.has_param("aux_star_mode"))
      cfg_->auxClockStarMode =
          StringUtils::safe_stoi(req.get_param_value("aux_star_mode"));

    if (req.has_param("side_panel_mode")) {
      std::string m = req.get_param_value("side_panel_mode");
      if (m == "dx_cluster") {
        cfg_->pane5Rotation = {WidgetType::DX_CLUSTER};
        cfg_->pane6Rotation = {};
      } else if (m == "on_the_air") {
        cfg_->pane5Rotation = {WidgetType::ON_THE_AIR};
        cfg_->pane6Rotation = {};
      } else if (m == "live_spots") {
        cfg_->pane5Rotation = {WidgetType::LIVE_SPOTS};
        cfg_->pane6Rotation = {};
      } else {
        cfg_->pane5Rotation = {WidgetType::DE_INFO};
        cfg_->pane6Rotation = {WidgetType::DX_INFO};
      }
    }
    if (req.has_param("panel_mode"))
      cfg_->panelMode = req.get_param_value("panel_mode");
    if (req.has_param("selected_satellite"))
      cfg_->selectedSatellite = req.get_param_value("selected_satellite");
    if (req.has_param("display_power_method"))
      cfg_->displayPowerMethod = req.get_param_value("display_power_method");
    if (req.has_param("lat"))
      cfg_->lat = StringUtils::safe_stod(req.get_param_value("lat"));
    if (req.has_param("lon"))
      cfg_->lon = StringUtils::safe_stod(req.get_param_value("lon"));
    if (req.has_param("audio_muted"))
      cfg_->audioMuted = req.get_param_value("audio_muted") == "1";
    if (req.has_param("font_path"))
      cfg_->fontPath = req.get_param_value("font_path");

    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/api/satellites",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (satMgr_) {
              for (const auto &s : satMgr_->getSatellites()) {
                j.push_back(s.name);
              }
              for (const auto &s : satMgr_->getCustomSatellites()) {
                j.push_back(s.name);
              }
            }
            res.set_content(j.dump(), "application/json");
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
                 std::lock_guard<std::mutex> lk(dataMutex_);
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

  svr.Get("/api/fonts", [](const httplib::Request &, httplib::Response &res) {
    nlohmann::json j = nlohmann::json::array();
    nlohmann::json builtin;
    builtin["name"] = "Built-in (Default)";
    builtin["path"] = "";
    j.push_back(builtin);
#ifndef __EMSCRIPTEN__
    std::vector<std::pair<std::string, std::string>> fonts;
    std::vector<std::string> dirs = {"/usr/share/fonts"};
    const char *home = std::getenv("HOME");
    if (home) {
      dirs.push_back(std::string(home) + "/.local/share/fonts");
      dirs.push_back(std::string(home) + "/.fonts");
    }
    for (const auto &dir : dirs) {
      std::error_code ec;
      if (!std::filesystem::exists(dir, ec))
        continue;
      for (auto &entry :
           std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec)
          continue;
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".ttf" || ext == ".otf") {
          fonts.push_back(
              {entry.path().stem().string(), entry.path().string()});
        }
      }
    }
    std::sort(fonts.begin(), fonts.end());
    for (const auto &[name, path] : fonts) {
      nlohmann::json f;
      f["name"] = name;
      f["path"] = path;
      j.push_back(f);
    }
#endif
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/api/display/status", [this](const httplib::Request &,
                                        httplib::Response &res) {
    nlohmann::json j;
    float fps = 0.0f;
    if (state_) {
      std::lock_guard<std::mutex> lk(state_->locationMutex);
      fps = state_->fps;
    }
    j["fps"] = std::to_string(fps).substr(0, 4);
    j["uptime"] =
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now() - startTime_)
                           .count()) +
        "s";
    j["power"] =
        displayPower_ ? (displayPower_->getPower() ? "on" : "off") : "unknown";
    j["method"] =
        displayPower_ ? displayPower_->getSelectedMethodName() : "none";
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
    auto prom = std::make_shared<std::promise<std::string>>();
    auto fut = prom->get_future();
    {
      std::lock_guard<std::mutex> lk(dataMutex_);
      if (netMgr_) {
        netMgr_->fetchAsync(
            targetUrl, [prom](std::string b) { prom->set_value(std::move(b)); },
            3600);
      } else {
        res.status = 503;
        return;
      }
    }
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

  svr.Get("/api/hub/dxcluster", [this](const httplib::Request &,
                                       httplib::Response &res) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    if (!cfg_ || cfg_->hubMode != HubMode::Master || !dxc_) {
      res.status = 403;
      return;
    }
    auto snap = dxc_->snapshot();
    auto now = std::chrono::system_clock::now();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &s : snap->spots) {
      auto age =
          std::chrono::duration_cast<std::chrono::minutes>(now - s.spottedAt)
              .count();
      if (age >= 60)
        continue;
      nlohmann::json j;
      j["txCall"] = s.txCall;
      j["txGrid"] = s.txGrid;
      j["rxCall"] = s.rxCall;
      j["rxGrid"] = s.rxGrid;
      j["mode"] = s.mode;
      j["freqKhz"] = s.freqKhz;
      j["snr"] = s.snr;
      j["txLat"] = s.txLat;
      j["txLon"] = s.txLon;
      j["rxLat"] = s.rxLat;
      j["rxLon"] = s.rxLon;
      j["txDxcc"] = s.txDxcc;
      j["rxDxcc"] = s.rxDxcc;
      j["spottedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                           s.spottedAt.time_since_epoch())
                           .count();
      arr.push_back(j);
    }
    res.set_content(arr.dump(), "application/json");
  });

  // --- Live Web View ---

  svr.Get("/live/status",
          [this](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j;
            j["liveWebEnabled"] = liveWebEnabled_;
            res.set_content(j.dump(), "application/json");
          });

  svr.Get("/live/touch", [this](const httplib::Request &req,
                                httplib::Response &res) {
    std::unique_lock<std::mutex> lk(dataMutex_);
    if (!liveWebEnabled_) {
      res.status = 403;
      return;
    }
    int x = StringUtils::safe_stoi(req.get_param_value("x"));
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    int outW = LOGICAL_WIDTH, outH = LOGICAL_HEIGHT;
    if (renderer_)
      SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    float layScale = std::min(static_cast<float>(outW) / LOGICAL_WIDTH,
                              static_cast<float>(outH) / LOGICAL_HEIGHT);
    int lx = (layScale > 0.0f) ? static_cast<int>(x / layScale) : x;
    int ly = (layScale > 0.0f) ? static_cast<int>(y / layScale) : y;
    SDL_Event e{};
    e.type = AE_BASE_EVENT + AE_TOUCH;
    e.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(lx));
    e.user.data2 = reinterpret_cast<void *>(static_cast<intptr_t>(ly));
    SDL_PushEvent(&e);
    if (frameCapture_)
      frameCapture_->requestBaseCapture();
    lk.unlock();
    res.set_content("ok", "text/plain");
  });

  svr.Get("/live/wheel",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::unique_lock<std::mutex> lk(dataMutex_);
            if (!liveWebEnabled_) {
              res.status = 403;
              return;
            }
            int y = StringUtils::safe_stoi(req.get_param_value("y"));
            SDL_Event e{};
            e.type = SDL_MOUSEWHEEL;
            e.wheel.y = y;
            SDL_PushEvent(&e);
            if (frameCapture_)
              frameCapture_->requestBaseCapture();
            lk.unlock();
            res.set_content("ok", "text/plain");
          });

  svr.Get("/live/key",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::unique_lock<std::mutex> lk(dataMutex_);
            if (!liveWebEnabled_) {
              res.status = 403;
              return;
            }
            std::string key = req.get_param_value("key");
            bool ctrl = req.get_param_value("ctrl") == "1";
            bool shift = req.get_param_value("shift") == "1";
            SDL_Scancode sc = SDL_GetScancodeFromName(key.c_str());
            SDL_Keymod mod = static_cast<SDL_Keymod>((ctrl ? KMOD_CTRL : 0) |
                                                     (shift ? KMOD_SHIFT : 0));
            SDL_Event down{}, up{};
            down.type = SDL_KEYDOWN;
            down.key.keysym.scancode = sc;
            down.key.keysym.mod = mod;
            up = down;
            up.type = SDL_KEYUP;
            SDL_PushEvent(&down);
            SDL_PushEvent(&up);
            if (frameCapture_)
              frameCapture_->requestBaseCapture();
            lk.unlock();
            res.set_content("ok", "text/plain");
          });

  svr.Get("/live/mouse",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::unique_lock<std::mutex> lk(dataMutex_);
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
            lk.unlock();
            res.set_content("ok", "text/plain");
          });

  // /api/live/vectors — spot + projection data for canvas overlay
  svr.Get("/api/live/vectors", [this](const httplib::Request &,
                                      httplib::Response &res) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    if (!cfg_ || !state_) {
      res.status = 503;
      return;
    }
    nlohmann::json j;
    j["projection"] = cfg_->projection;
    {
      std::lock_guard<std::mutex> lk_state(state_->locationMutex);
      j["deLat"] = state_->deLocation.lat;
      j["deLon"] = state_->deLocation.lon;
      j["dxLat"] = state_->dxLocation.lat;
      j["dxLon"] = state_->dxLocation.lon;
    }
    // Logical dimensions so the browser can compute the pixel scale factor.
    j["logicalW"] = 800;
    j["logicalH"] = 480;

    // Compute mapRect in logical 800×480 coordinates.
    // Map widget is at {139,149,660,330} per LayoutManager::kMain.
    {
      constexpr int wx = 139, wy = 149, ww = 660, wh = 330;
      int mx, my, mw, mh;
      const std::string &proj = cfg_->projection;
      if (proj == "azimuthal") {
        int side = std::min(ww, wh);
        mx = wx + (ww - side) / 2;
        my = wy + (wh - side) / 2;
        mw = side;
        mh = side;
      } else if (proj == "dual_azimuthal") {
        mx = wx;
        my = wy;
        mw = ww;
        mh = wh;
      } else {
        // equirectangular / robinson / mercator — 2:1 aspect
        int mapW = ww, mapH = ww / 2;
        if (mapH > wh) {
          mapH = wh;
          mapW = mapH * 2;
        }
        mx = wx + (ww - mapW) / 2;
        my = wy + (wh - mapH) / 2;
        mw = mapW;
        mh = mapH;
      }
      j["mapRect"] = {{"x", mx}, {"y", my}, {"w", mw}, {"h", mh}};
    }

    // DX cluster spots (< 60 min old)
    nlohmann::json dxArr = nlohmann::json::array();
    if (dxc_) {
      auto snap = dxc_->snapshot();
      auto now = std::chrono::system_clock::now();
      for (const auto &s : snap->spots) {
        auto age =
            std::chrono::duration_cast<std::chrono::minutes>(now - s.spottedAt)
                .count();
        if (age >= 60)
          continue;
        nlohmann::json sj;
        sj["txCall"] = s.txCall;
        sj["txLat"] = s.txLat;
        sj["txLon"] = s.txLon;
        sj["rxCall"] = s.rxCall;
        sj["rxLat"] = s.rxLat;
        sj["rxLon"] = s.rxLon;
        sj["mode"] = s.mode;
        sj["freqKhz"] = s.freqKhz;
        sj["spottedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
                              s.spottedAt.time_since_epoch())
                              .count();
        dxArr.push_back(sj);
      }
    }
    j["dxSpots"] = dxArr;

    // Live spots — convert Maidenhead receiver grid to lat/lon centre
    nlohmann::json liveArr = nlohmann::json::array();
    if (spots_) {
      auto snap = spots_->snapshot();
      for (const auto &s : snap->spots) {
        const std::string &g = s.receiverGrid;
        if (g.size() < 4)
          continue;
        char f0 = std::toupper((unsigned char)g[0]);
        char f1 = std::toupper((unsigned char)g[1]);
        char f2 = g[2];
        char f3 = g[3];
        if (f0 < 'A' || f0 > 'R' || f1 < 'A' || f1 > 'R' || f2 < '0' ||
            f2 > '9' || f3 < '0' || f3 > '9')
          continue;
        double rxLon = (f0 - 'A') * 20.0 - 180.0 + (f2 - '0') * 2.0 + 1.0;
        double rxLat = (f1 - 'A') * 10.0 - 90.0 + (f3 - '0') * 1.0 + 0.5;
        nlohmann::json sj;
        sj["rxLat"] = rxLat;
        sj["rxLon"] = rxLon;
        sj["freqKhz"] = s.freqKhz;
        liveArr.push_back(sj);
      }
    }
    j["liveSpots"] = liveArr;

    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content(j.dump(), "application/json");
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
    #feed-wrap { position: relative; display: inline-block; max-width: 100vw; line-height: 0; }
    #feed { max-width: 100%; display: block; cursor: crosshair; }
    #vcv { position: absolute; top: 0; left: 0; pointer-events: none; }
  </style>
</head>
<body>
  <div id="info">Click map to interact &bull; Scroll to zoom &bull; <a href="/" style="color:#0f0">Config</a></div>
  <div id="feed-wrap">
    <img id="feed" src="/stream.mjpeg">
    <canvas id="vcv"></canvas>
  </div>
  <script>
    const img = document.getElementById('feed');
    const canvas = document.getElementById('vcv');
    const ctx2d = canvas.getContext('2d');
    const VM = new URLSearchParams(window.location.search).get('vectors') === '1';
    if (VM) img.src = '/stream.mjpeg?vectors=1';

    function syncCanvas() {
      canvas.width = img.naturalWidth || 800;
      canvas.height = img.naturalHeight || 480;
      canvas.style.width = img.clientWidth + 'px';
      canvas.style.height = img.clientHeight + 'px';
    }
    img.addEventListener('load', () => { syncCanvas(); if (VM && lastData) drawSpots(lastData); });
    window.addEventListener('resize', () => { syncCanvas(); if (VM && lastData) drawSpots(lastData); });

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

    // ---- Vector overlay (active when ?vectors=1) ----
    var lastData = null;
    if (VM) {
      // Robinson projection coefficients (19 entries, 0-90° in 5° steps)
      const ROB = [
        [1.0000,0.0000],[0.9986,0.0620],[0.9954,0.1240],[0.9900,0.1860],
        [0.9822,0.2480],[0.9730,0.3100],[0.9600,0.3720],[0.9427,0.4340],
        [0.9216,0.4958],[0.8962,0.5571],[0.8679,0.6176],[0.8350,0.6769],
        [0.7986,0.7346],[0.7597,0.7903],[0.7186,0.8435],[0.6732,0.8936],
        [0.6213,0.9394],[0.5722,0.9761],[0.5322,1.0000]
      ];
      function projRobinson(lat, lon, mr) {
        const a=Math.abs(lat), i=Math.min(Math.floor(a/5),17), f=a/5-i;
        const xc=ROB[i][0]+f*(ROB[i+1][0]-ROB[i][0]);
        const yc=ROB[i][1]+f*(ROB[i+1][1]-ROB[i][1]);
        const rnx=(lon/180)*xc, rny=lat<0?-yc:yc;
        return {x:mr.x+(rnx+1)*0.5*mr.w, y:mr.y+(1-rny)*0.5*mr.h};
      }
      function projEquirect(lat, lon, mr) {
        return {x:mr.x+(lon+180)/360*mr.w, y:mr.y+(90-lat)/180*mr.h};
      }
      function azNxy(lat, lon, lat0, lon0) {
        const D=Math.PI/180;
        const phi=lat*D, phi0=lat0*D, dl=(lon-lon0)*D;
        const cc=Math.max(-1,Math.min(1,Math.sin(phi0)*Math.sin(phi)+Math.cos(phi0)*Math.cos(phi)*Math.cos(dl)));
        const c=Math.acos(cc);
        if(c<1e-10) return {nx:0,ny:0};
        if(c>Math.PI-1e-10) return null;
        const k=c/Math.sin(c);
        return {nx:k*Math.cos(phi)*Math.sin(dl)/Math.PI,
                ny:k*(Math.cos(phi0)*Math.sin(phi)-Math.sin(phi0)*Math.cos(phi)*Math.cos(dl))/Math.PI};
      }
      function projAzimuthal(lat, lon, deLat, deLon, mr) {
        const p=azNxy(lat,lon,deLat,deLon); if(!p) return null;
        const sc=Math.min(mr.w,mr.h);
        return {x:mr.x+mr.w*0.5+p.nx*sc*0.5, y:mr.y+mr.h*0.5-p.ny*sc*0.5};
      }
      function projDualAz(lat, lon, deLat, deLon, mr) {
        const hw=mr.w/2, R=Math.min(hw,mr.h)*0.5;
        const cx=mr.x+hw*0.5, cy=mr.y+mr.h*0.5;
        const p=azNxy(lat,lon,deLat,deLon); if(!p) return null;
        return {x:cx+p.nx*R, y:cy-p.ny*R};
      }
      function projMercator(lat, lon, mr) {
        const mL=85.05112878, D=Math.PI/180;
        const cl=Math.max(-mL,Math.min(mL,lat));
        const mY=Math.log(Math.tan(Math.PI/4+cl*D/2));
        const mmY=Math.log(Math.tan(Math.PI/4+mL*D/2));
        return {x:mr.x+(lon+180)/360*mr.w, y:mr.y+(0.5-0.5*mY/mmY)*mr.h};
      }
      function project(lat, lon, proj, deLat, deLon, mr) {
        switch(proj) {
          case 'robinson':      return projRobinson(lat,lon,mr);
          case 'azimuthal':     return projAzimuthal(lat,lon,deLat,deLon,mr);
          case 'dual_azimuthal':return projDualAz(lat,lon,deLat,deLon,mr);
          case 'mercator':      return projMercator(lat,lon,mr);
          default:              return projEquirect(lat,lon,mr);
        }
      }

      const BC = {
        '160m':'#FF4444','80m':'#FF8800','60m':'#FFAA00','40m':'#FFFF00',
        '30m':'#88FF00','20m':'#00FF00','17m':'#00FFAA','15m':'#00FFFF',
        '12m':'#00AAFF','10m':'#0044FF','6m':'#8800FF','2m':'#FF00FF'
      };
      function freqBand(f) {
        if(f>=1800&&f<=2000)   return '160m';
        if(f>=3500&&f<=4000)   return '80m';
        if(f>=5330&&f<=5410)   return '60m';
        if(f>=7000&&f<=7300)   return '40m';
        if(f>=10100&&f<=10150) return '30m';
        if(f>=14000&&f<=14350) return '20m';
        if(f>=18068&&f<=18168) return '17m';
        if(f>=21000&&f<=21450) return '15m';
        if(f>=24890&&f<=24990) return '12m';
        if(f>=28000&&f<=29700) return '10m';
        if(f>=50000&&f<=54000) return '6m';
        if(f>=144000&&f<=148000) return '2m';
        return null;
      }

      function scaledMr(mr, d) {
        // Scale mapRect from logical coords to canvas pixel coords
        const sx = canvas.width / d.logicalW;
        const sy = canvas.height / d.logicalH;
        return {x:mr.x*sx, y:mr.y*sy, w:mr.w*sx, h:mr.h*sy};
      }

      function drawSpots(d) {
        lastData = d;
        ctx2d.clearRect(0, 0, canvas.width, canvas.height);
        const mr = scaledMr(d.mapRect, d);
        const proj = d.projection, deLat = d.deLat, deLon = d.deLon;

        // Live spots — small semi-transparent dots at receiver grid centre
        for (const s of d.liveSpots) {
          if (s.rxLat == null) continue;
          const pt = project(s.rxLat, s.rxLon, proj, deLat, deLon, mr);
          if (!pt) continue;
          const band = freqBand(s.freqKhz);
          const col = (band && BC[band]) ? BC[band] : '#aaaaaa';
          ctx2d.beginPath();
          ctx2d.arc(pt.x, pt.y, 2, 0, 2*Math.PI);
          ctx2d.fillStyle = col + '80';
          ctx2d.fill();
        }

        // DX cluster spots — labelled dots
        for (const s of d.dxSpots) {
          if (!s.txLat && !s.txLon) continue;
          const pt = project(s.txLat, s.txLon, proj, deLat, deLon, mr);
          if (!pt) continue;
          const band = freqBand(s.freqKhz);
          const col = (band && BC[band]) ? BC[band] : '#ffffff';
          ctx2d.beginPath();
          ctx2d.arc(pt.x, pt.y, 4, 0, 2*Math.PI);
          ctx2d.fillStyle = col;
          ctx2d.fill();
          ctx2d.font = '9px monospace';
          ctx2d.fillStyle = col;
          ctx2d.fillText(s.txCall, pt.x + 6, pt.y + 3);
        }
      }

      function pollVectors() {
        fetch('/api/live/vectors')
          .then(r => r.json())
          .then(d => { syncCanvas(); drawSpots(d); })
          .catch(e => console.warn('vectors poll:', e));
      }
      pollVectors();
      setInterval(pollVectors, 15000);
    }
  </script>
</body>
</html>)HTML";
    res.set_content(html, "text/html");
  });

  svr.Get("/stream.mjpeg", [this](const httplib::Request &req,
                                  httplib::Response &res) {
    if (!frameCapture_ || !liveWebEnabled_) {
      res.status = 503;
      return;
    }
    bool vectorsMode =
        req.has_param("vectors") && req.get_param_value("vectors") == "1";
    if (vectorsMode) {
      // Base-layer stream: same frame content but throttled to ~1 FPS.
      // Full spot suppression from the MJPEG base is deferred (requires
      // MapWidget render-loop refactoring); spots are also drawn by the
      // browser canvas overlay.
      {
        std::lock_guard<std::mutex> lk(dataMutex_);
        if (frameCapture_)
          frameCapture_->addBaseSubscriber();
      }
      uint64_t seq = 0;
      res.set_content_provider(
          "multipart/x-mixed-replace;boundary=hamclock",
          [this, seq](size_t /*offset*/,
                      httplib::DataSink &sink) mutable -> bool {
            if (!sink.is_writable())
              return false;
            uint64_t outSeq = 0;
            FrameCapture *fc;
            {
              std::lock_guard<std::mutex> lk(dataMutex_);
              fc = frameCapture_;
            }
            if (!fc)
              return false;
            auto frame = fc->waitBaseFrame(seq, 10000, outSeq);
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
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (frameCapture_)
              frameCapture_->removeBaseSubscriber();
          });
    } else {
      {
        std::lock_guard<std::mutex> lk(dataMutex_);
        if (frameCapture_)
          frameCapture_->addSubscriber();
      }
      uint64_t seq = 0;
      res.set_content_provider(
          "multipart/x-mixed-replace;boundary=hamclock",
          [this, seq](size_t /*offset*/,
                      httplib::DataSink &sink) mutable -> bool {
            if (!sink.is_writable())
              return false;
            uint64_t outSeq = 0;
            FrameCapture *fc;
            {
              std::lock_guard<std::mutex> lk(dataMutex_);
              fc = frameCapture_;
            }
            if (!fc)
              return false;
            auto frame = fc->waitFrame(seq, 5000, outSeq);
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
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (frameCapture_)
              frameCapture_->removeSubscriber();
          });
    }
  });

  // --- Legacy Data Getters ---

  svr.Get("/get_config.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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
            struct tm utc{};
            Astronomy::portable_gmtime(&now, &utc);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                          utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                          utc.tm_hour, utc.tm_min, utc.tm_sec);
            std::ostringstream oss;
            oss << "Clock_UTC  " << buf << "\n";
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_de.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::ostringstream oss;
            {
              std::lock_guard<std::mutex> lk(state_->locationMutex);
              oss << "DE_Callsign  " << state_->deCallsign << "\n";
              oss << "DE_Grid      " << state_->deGrid << "\n";
              oss << "DE_Lat       " << state_->deLocation.lat << "\n";
              oss << "DE_Lon       " << state_->deLocation.lon << "\n";
            }
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_spacewx.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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

  svr.Get("/get_sys.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::ostringstream oss;
            if (cpu_) {
              oss << "CPU_Temp_C  " << cpu_->getTemperature() << "\n";
              oss << "CPU_Temp_F  " << cpu_->getTemperatureF() << "\n";
            }
            auto upSec = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now() - startTime_)
                             .count();
            oss << "Uptime_s    " << upSec << "\n";
            if (state_) {
              std::lock_guard<std::mutex> lk(state_->locationMutex);
              oss << "FPS         " << state_->fps << "\n";
            }
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_contests.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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

  svr.Get("/get_livespots.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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
              oss << std::left << std::setw(12) << s.senderCallsign
                  << std::setw(12) << s.receiverGrid << s.freqKhz << "\n";
            }
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_livestats.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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

  svr.Get("/get_satellites.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (!satMgr_) {
              res.status = 503;
              return;
            }
            auto sats = satMgr_->getSatellites();
            std::ostringstream oss;
            for (const auto &s : sats) oss << s.name << "\n";
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_ontheair.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
            if (!activityStore_) {
              res.status = 503;
              return;
            }
            ActivityData ad = activityStore_->get();
            std::ostringstream oss;
            for (const auto &s : ad.ontaSpots)
              oss << std::left << std::setw(12) << s.call << std::setw(8)
                  << s.program << std::setw(10) << s.ref << s.freqKhz << "\n";
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/get_dxpeds.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
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
    FrameCapture *fc;
    {
      std::lock_guard<std::mutex> lk(dataMutex_);
      fc = frameCapture_;
    }
    if (!fc) {
      res.status = 503;
      return;
    }

    // Increment subscribers so FrameCapture::capture() actually runs on the
    // main thread
    fc->addSubscriber();

    std::string seqParam = req.get_param_value("seq");
    uint64_t afterSeq = 0;
    if (seqParam.empty()) {
      // If no seq is provided, wait for the NEXT frame after the current one.
      // This ensures we don't get a stale cached frame from when nobody was
      // watching.
      afterSeq = fc->latestSeq();
    } else {
      afterSeq = static_cast<uint64_t>(StringUtils::safe_stoi(seqParam));
    }
    uint64_t outSeq = 0;

    // Increased timeout to 1000ms to allow for RPi 3B scheduling and encoding
    auto frame = fc->waitFrame(afterSeq, 1000, outSeq);

    fc->removeSubscriber();

    if (frame.empty()) {
      res.status = 503;
      return;
    }
    res.set_header("X-Frame-Seq", std::to_string(outSeq));
    res.set_content(
        std::string(reinterpret_cast<const char *>(frame.data()), frame.size()),
        "image/jpeg");
  });

  // /live.jpg — legacy alias for /get_capture; used by original HamClock
  // clients and by the MCP verification tool.
  svr.Get("/live.jpg", [this](const httplib::Request &req,
                              httplib::Response &res) {
    FrameCapture *fc;
    {
      std::lock_guard<std::mutex> lk(dataMutex_);
      fc = frameCapture_;
    }
    if (!fc) {
      res.status = 503;
      return;
    }
    fc->addSubscriber();
    std::string seqParam = req.get_param_value("seq");
    uint64_t afterSeq =
        seqParam.empty()
            ? fc->latestSeq()
            : static_cast<uint64_t>(StringUtils::safe_stoi(seqParam));
    uint64_t outSeq = 0;
    auto frame = fc->waitFrame(afterSeq, 1000, outSeq);
    fc->removeSubscriber();
    if (frame.empty()) {
      res.status = 503;
      return;
    }
    res.set_header("X-Frame-Seq", std::to_string(outSeq));
    res.set_content(
        std::string(reinterpret_cast<const char *>(frame.data()), frame.size()),
        "image/jpeg");
  });

  // /set_touch?x=N&y=N — legacy alias for /live/touch; injects a tap event
  // at logical (x,y) coordinates.  Does not require liveWebEnabled_.
  svr.Get("/set_touch", [this](const httplib::Request &req,
                               httplib::Response &res) {
    int x = StringUtils::safe_stoi(req.get_param_value("x"));
    int y = StringUtils::safe_stoi(req.get_param_value("y"));
    std::lock_guard<std::mutex> lk(dataMutex_);
    int outW = LOGICAL_WIDTH, outH = LOGICAL_HEIGHT;
    if (renderer_)
      SDL_GetRendererOutputSize(renderer_, &outW, &outH);
    float layScale = std::min(static_cast<float>(outW) / LOGICAL_WIDTH,
                              static_cast<float>(outH) / LOGICAL_HEIGHT);
    int lx = (layScale > 0.0f) ? static_cast<int>(x / layScale) : x;
    int ly = (layScale > 0.0f) ? static_cast<int>(y / layScale) : y;
    SDL_Event e{};
    e.type = AE_BASE_EVENT + AE_TOUCH;
    e.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(lx));
    e.user.data2 = reinterpret_cast<void *>(static_cast<intptr_t>(ly));
    SDL_PushEvent(&e);
    if (frameCapture_)
      frameCapture_->requestBaseCapture();
    res.set_content("ok", "text/plain");
  });

  // /set_char?val=N — legacy endpoint; injects a keystroke for ASCII code N.
  // Printable characters (32–126) push an SDL_TEXTINPUT event so text fields
  // receive them; control codes (Backspace=8, Tab=9, Enter=13, Esc=27) push
  // SDL_KEYDOWN/KEYUP.
  svr.Get("/set_char",
          [this](const httplib::Request &req, httplib::Response &res) {
            int val = StringUtils::safe_stoi(req.get_param_value("val"));
            if (val >= 32 && val <= 126) {
              SDL_Event te{};
              te.type = SDL_TEXTINPUT;
              te.text.text[0] = static_cast<char>(val);
              te.text.text[1] = '\0';
              SDL_PushEvent(&te);
            } else {
              SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
              switch (val) {
                case 8:
                  sc = SDL_SCANCODE_BACKSPACE;
                  break;
                case 9:
                  sc = SDL_SCANCODE_TAB;
                  break;
                case 13:
                  sc = SDL_SCANCODE_RETURN;
                  break;
                case 27:
                  sc = SDL_SCANCODE_ESCAPE;
                  break;
                default:
                  break;
              }
              if (sc != SDL_SCANCODE_UNKNOWN) {
                SDL_Event down{}, up{};
                down.type = SDL_KEYDOWN;
                down.key.keysym.scancode = sc;
                up = down;
                up.type = SDL_KEYUP;
                SDL_PushEvent(&down);
                SDL_PushEvent(&up);
              }
            }
            if (frameCapture_)
              frameCapture_->requestBaseCapture();
            res.set_content("ok", "text/plain");
          });

  svr.Get("/get_dx.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            bool dxActive;
            std::string dxGrid;
            LatLon deLoc, dxLoc;
            {
              std::lock_guard<std::mutex> lk(state_->locationMutex);
              dxActive = state_->dxActive;
              dxGrid = state_->dxGrid;
              deLoc = state_->deLocation;
              dxLoc = state_->dxLocation;
            }
            if (!dxActive) {
              res.set_content("DX not set\n", "text/plain");
              return;
            }
            std::ostringstream oss;
            oss << "DX_Grid      " << dxGrid << "\n";
            oss << "DX_Lat       " << dxLoc.lat << "\n";
            oss << "DX_Lon       " << dxLoc.lon << "\n";
            double distKm = Astronomy::calculateDistance(deLoc, dxLoc);
            double bearing = Astronomy::calculateBearing(deLoc, dxLoc);
            oss << "DX_Dist_km   " << static_cast<int>(distKm) << "\n";
            oss << "DX_Bearing   " << static_cast<int>(bearing) << "\n";
            res.set_content(oss.str(), "text/plain");
          });

  // --- Legacy Control Setters ---

  svr.Get("/set_screenlock",
          [this](const httplib::Request &req, httplib::Response &res) {
            screenLocked_ = (req.get_param_value("lock") == "on");
            res.set_content("ok", "text/plain");
          });

  svr.Get("/set_mappos",
          [this](const httplib::Request &req, httplib::Response &res) {
            double lat = StringUtils::safe_stod(req.get_param_value("lat"));
            double lon = StringUtils::safe_stod(req.get_param_value("lon"));
            std::string target = req.get_param_value("target");
            std::string grid = Astronomy::latLonToGrid(lat, lon);
            {
              std::lock_guard<std::mutex> lk(state_->locationMutex);
              if (target == "dx") {
                state_->dxLocation = {lat, lon};
                state_->dxGrid = grid;
                state_->dxActive = true;
              } else {
                state_->deLocation = {lat, lon};
                state_->deGrid = grid;
              }
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
    {
      std::lock_guard<std::mutex> lk(state_->locationMutex);
      state_->deLocation = {lat, lon};
      state_->deGrid = Astronomy::latLonToGrid(lat, lon);
      cfg_->lat = lat;
      cfg_->lon = lon;
      cfg_->grid = state_->deGrid;
    }
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
    {
      std::lock_guard<std::mutex> lk(state_->locationMutex);
      state_->dxLocation = {lat, lon};
      state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
      state_->dxActive = true;
    }
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_cluster", [this](const httplib::Request &req,
                                 httplib::Response &res) {
    if (req.has_param("host"))
      cfg_->dxClusterHost = req.get_param_value("host");
    if (req.has_param("port"))
      cfg_->dxClusterPort = StringUtils::safe_stoi(req.get_param_value("port"));
    if (req.has_param("user"))
      cfg_->dxClusterLogin = req.get_param_value("user");
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get("/set_title",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get("/set_theme",
          [this](const httplib::Request &req, httplib::Response &res) {
            if (!req.has_param("theme")) {
              res.status = 400;
              return;
            }
            cfg_->theme = req.get_param_value("theme");
            if (cfgMgr_)
              cfgMgr_->save(*cfg_);
            if (reloadFlag_)
              reloadFlag_->store(true, std::memory_order_release);
            res.set_content("ok", "text/plain");
          });

#ifdef __linux__
  svr.Get("/set_time", [](const httplib::Request &req, httplib::Response &res) {
    struct timeval tv{};
    if (req.has_param("unix")) {
      tv.tv_sec = static_cast<time_t>(
          StringUtils::safe_stoi(req.get_param_value("unix")));
    } else if (req.has_param("ISO")) {
      std::string iso = req.get_param_value("ISO");
      struct tm t{};
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

  svr.Get("/set_alarm",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get("/set_once_alarm",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get(
      "/get_env.txt", [this](const httplib::Request &, httplib::Response &res) {
        if (!weatherStore_) {
          res.status = 503;
          return;
        }
        WeatherData d = weatherStore_->get();
        if (!d.valid) {
          res.status = 503;
          return;
        }
        // Magnus formula dewpoint (valid -40..60 C)
        constexpr float a = 17.62f, b = 243.12f;
        float alpha = a * d.temp / (b + d.temp) + std::log(d.humidity / 100.0f);
        float dp = b * alpha / (a - alpha);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1);
        oss << "Temp_C     " << d.temp << "\n";
        oss << "Pressure_hPa " << d.pressure << "\n";
        oss << "Humidity_pct " << d.humidity << "\n";
        oss << "Dewpoint_C " << dp << "\n";
        res.set_content(oss.str(), "text/plain");
      });

  svr.Get("/get_stopwatch.txt", [this](const httplib::Request &,
                                       httplib::Response &res) {
    std::lock_guard<std::mutex> lk(dataMutex_);
    if (!stopwatch_) {
      res.status = 503;
      return;
    }
    using ms = std::chrono::milliseconds;
    auto elapsedMs =
        std::chrono::duration_cast<ms>(stopwatch_->elapsed()).count();
    std::ostringstream oss;
    oss << "Running    " << (stopwatch_->isRunning() ? "yes" : "no") << "\n";
    oss << "Elapsed_ms " << elapsedMs << "\n";
    oss << "Lap_count  " << stopwatch_->laps().size() << "\n";
    for (size_t i = 0; i < stopwatch_->laps().size(); ++i) {
      auto lapMs =
          std::chrono::duration_cast<ms>(stopwatch_->laps()[i]).count();
      oss << "Lap_" << (i + 1) << "     " << lapMs << "\n";
    }
    res.set_content(oss.str(), "text/plain");
  });

  svr.Get("/get_satellite.txt",
          [this](const httplib::Request &, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
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
            auto tle = satMgr_->findByName(name);
            if (tle) {
              Satellite sat(*tle);
              double deLat, deLon;
              {
                std::lock_guard<std::mutex> lk_state(state_->locationMutex);
                deLat = state_->deLocation.lat;
                deLon = state_->deLocation.lon;
              }
              sat.setObserver(deLat, deLon);
              SatObservation obs = sat.predict();
              oss << "Az         " << static_cast<int>(obs.azimuth) << "\n";
              oss << "El         " << static_cast<int>(obs.elevation) << "\n";
              oss << "Range_km   " << static_cast<int>(obs.range) << "\n";
              oss << "Visible    " << (obs.visible ? "yes" : "no") << "\n";
            }
            res.set_content(oss.str(), "text/plain");
          });

  svr.Get("/set_satname",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
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

  svr.Get("/set_sattle",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::lock_guard<std::mutex> lk(dataMutex_);
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

  svr.Get("/get_active_pane.txt",
          [this](const httplib::Request &, httplib::Response &res) {
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

  svr.Get("/get_pane.txt",
          [this](const httplib::Request &req, httplib::Response &res) {
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
  svr.Get("/set_projection",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get(
      "/set_pane", [this](const httplib::Request &req, httplib::Response &res) {
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
        } else if (action == "solo" && !widget.empty()) {
          WidgetType wt = widgetTypeFromString(widget, WidgetType::SOLAR);
          pane->setRotation({wt}, cfg_->rotationIntervalS, cfg_->syncRotation);
          pane->forceAdvance();
        }
        res.set_content("ok", "text/plain");
      });

  svr.Get("/get_capabilities", [](const httplib::Request &,
                                  httplib::Response &res) {
    nlohmann::json j;

    nlohmann::json wArr = nlohmann::json::array();
    for (WidgetType wt : getAllBaseWidgetTypes())
      wArr.push_back(widgetTypeToString(wt));
    j["widgets"] = wArr;

    j["projections"] =
        nlohmann::json::array({"equirectangular", "robinson", "azimuthal",
                               "mercator", "dual_azimuthal"});

    nlohmann::json tArr = nlohmann::json::array();
    for (const auto &t : getAvailableThemes())
      if (t != "custom")
        tArr.push_back(t);
    j["themes"] = tArr;

    j["prop_overlays"] =
        nlohmann::json::array({"none", "muf", "voacap", "reliability", "toa",
                               "heatmap", "drap", "aurora"});

    j["wx_overlays"] = nlohmann::json::array({"none", "wxmb", "clouds_grib"});

    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/set_displayTimes", [this](const httplib::Request &req,
                                      httplib::Response &res) {
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

  svr.Get("/set_rotation",
          [this](const httplib::Request &req, httplib::Response &res) {
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
      cfg_->rotatorPort = StringUtils::safe_stoi(req.get_param_value("port"));
    if (cfgMgr_)
      cfgMgr_->save(*cfg_);
    if (reloadFlag_)
      reloadFlag_->store(true, std::memory_order_release);
    res.set_content("ok", "text/plain");
  });

  svr.Get(
      "/screen", [this](const httplib::Request &req, httplib::Response &res) {
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

  // --- Propagation, Debug, and Reload ---

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

  svr.Post("/api/reload",
           [this](const httplib::Request &, httplib::Response &res) {
             if (cfgMgr_)
               cfgMgr_->save(*cfg_);
             if (reloadFlag_)
               reloadFlag_->store(true, std::memory_order_release);
             res.set_content("ok", "text/plain");
           });

  svr.Post("/api/display/power",
           [this](const httplib::Request &req, httplib::Response &res) {
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
    {
      float _fps = 0.0f;
      if (state_) {
        std::lock_guard<std::mutex> lk(state_->locationMutex);
        _fps = state_->fps;
      }
      j["fps"] = _fps;
    }
    j["running_since"] = std::chrono::duration_cast<std::chrono::seconds>(
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
      std::lock_guard<std::mutex> lk(state_->servicesMutex);
      for (const auto &[name, st] : state_->services) {
        nlohmann::json sj;
        sj["ok"] = st.ok;
        sj["lastError"] = st.lastError;
        if (st.lastSuccess != std::chrono::system_clock::time_point{}) {
          std::time_t t = std::chrono::system_clock::to_time_t(st.lastSuccess);
          char buf[32];
          struct tm utc{};
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

  svr.Get("/debug/logs", [](const httplib::Request &, httplib::Response &res) {
    auto lines = Log::getRecentLogs();
    nlohmann::json j = nlohmann::json::array();
    for (const auto &line : lines) j.push_back(line);
    res.set_content(j.dump(), "application/json");
  });

  svr.Get("/get_build.txt",
          [](const httplib::Request &, httplib::Response &res) {
            SDL_version sdlLinked;
            SDL_GetVersion(&sdlLinked);
            std::ostringstream oss;
#if defined(__EMSCRIPTEN__)
            oss << "Platform   WASM\n";
#elif defined(_WIN32)
    oss << "Platform   Windows\n";
#elif defined(__APPLE__)
    oss << "Platform   macOS\n";
#else
    oss << "Platform   Linux\n";
#endif
#ifdef NDEBUG
            oss << "Build      release\n";
#else
    oss << "Build      debug\n";
#endif
            oss << "SDL_version " << static_cast<int>(sdlLinked.major) << "."
                << static_cast<int>(sdlLinked.minor) << "."
                << static_cast<int>(sdlLinked.patch) << "\n";
            res.set_content(oss.str(), "text/plain");
          });

#ifdef ENABLE_DEBUG_API
  svr.Get("/debug/widgets",
          [](const httplib::Request &, httplib::Response &res) {
            nlohmann::json j = nlohmann::json::array();
            auto snapshot = UIRegistry::getInstance().getSnapshot();
            for (const auto &[id, info] : snapshot) {
              nlohmann::json w;
              w["id"] = id;
              w["name"] = info.name;
              w["rect"] = {{"x", info.rect.x},
                           {"y", info.rect.y},
                           {"w", info.rect.w},
                           {"h", info.rect.h}};
              j.push_back(w);
            }
            res.set_content(j.dump(2), "application/json");
          });

  svr.Get("/debug/click",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get("/debug/keypress",
          [](const httplib::Request &req, httplib::Response &res) {
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

  svr.Get("/debug/watchlist/add",
          [this](const httplib::Request &req, httplib::Response &res) {
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

  // --- Calendar endpoints ---

  // POST /set_calendar.json — accepts a JSON array of event objects from an
  // external script (e.g. evolution-to-hamclock.py). Each object must have:
  //   "summary": string, "start": "YYYY-MM-DD HH:MM:SS", "end": ...,
  //   "source": string (optional).
  svr.Post("/set_calendar.json", [this](const httplib::Request &req,
                                        httplib::Response &res) {
    if (!calendarStore_) {
      res.status = 503;
      return;
    }
    try {
      auto j = nlohmann::json::parse(req.body);
      if (!j.is_array()) {
        res.status = 400;
        res.set_content("expected JSON array", "text/plain");
        return;
      }
      std::vector<CalendarEvent> events;
      for (const auto &item : j) {
        CalendarEvent ev;
        ev.summary = item.value("summary", std::string{});
        ev.source = item.value("source", std::string{});

        auto parseTime = [](const std::string &s) -> time_t {
          struct tm t{};
          if (std::sscanf(s.c_str(), "%d-%d-%d %d:%d:%d", &t.tm_year, &t.tm_mon,
                          &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec) != 6)
            return 0;
          t.tm_year -= 1900;
          t.tm_mon -= 1;
          return Astronomy::portable_timegm(&t);
        };

        ev.start = parseTime(item.value("start", std::string{}));
        ev.end = parseTime(item.value("end", std::string{}));
        if (ev.start == 0)
          continue;
        if (ev.end <= ev.start)
          ev.end = ev.start + 3600;

        // Detect all-day: midnight start, duration >= 23h
        struct tm st{};
        Astronomy::portable_gmtime(&ev.start, &st);
        ev.allDay = (st.tm_hour == 0 && st.tm_min == 0 && st.tm_sec == 0 &&
                     (ev.end - ev.start) >= 23 * 3600);

        events.push_back(std::move(ev));
      }
      calendarStore_->set(std::move(events));
      res.set_content("ok", "text/plain");
    } catch (const std::exception &e) {
      res.status = 400;
      res.set_content(e.what(), "text/plain");
    }
  });

  svr.Get("/get_calendar.json",
          [this](const httplib::Request &, httplib::Response &res) {
            if (!calendarStore_) {
              res.status = 503;
              return;
            }
            auto events = calendarStore_->snapshot();
            auto arr = nlohmann::json::array();
            for (const auto &ev : events) {
              nlohmann::json je;
              je["summary"] = ev.summary;
              je["source"] = ev.source;
              je["all_day"] = ev.allDay;

              auto fmtTime = [](time_t t) -> std::string {
                struct tm tm{};
                Astronomy::portable_gmtime(&t, &tm);
                char buf[72];
                std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                              tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                              tm.tm_hour, tm.tm_min, tm.tm_sec);
                return buf;
              };
              je["start"] = fmtTime(ev.start);
              je["end"] = fmtTime(ev.end);
              arr.push_back(je);
            }
            res.set_content(arr.dump(2), "application/json");
          });
}
