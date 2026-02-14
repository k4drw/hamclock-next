#include "ConfigManager.h"

#include <SDL.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>

static std::string colorToHex(SDL_Color c) {
  char buf[8];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
  return buf;
}

static SDL_Color hexToColor(const std::string &hex, SDL_Color fallback) {
  if (hex.size() < 7 || hex[0] != '#')
    return fallback;
  unsigned int r = 0, g = 0, b = 0;
  if (std::sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
    return {static_cast<Uint8>(r), static_cast<Uint8>(g), static_cast<Uint8>(b),
            255};
  }
  return fallback;
}

bool ConfigManager::init() {
  // Use SDL_GetPrefPath for cross-platform data directory
  // On Linux: ~/.local/share/HamClock/HamClock-Next/
  // On Windows: %APPDATA%\HamClock\HamClock-Next\
  // On MacOS: ~/Library/Application Support/HamClock/HamClock-Next/
  char *prefPath = SDL_GetPrefPath("HamClock", "HamClock-Next");
  if (!prefPath) {
    std::fprintf(stderr, "ConfigManager: SDL_GetPrefPath failed: %s\n",
                 SDL_GetError());
    return false;
  }

  configDir_ = std::filesystem::path(prefPath);
  SDL_free(prefPath);

  // Fallback for Linux: check ~/.config/hamclock if prefPath is empty or first
  // run This helps migration but primarily we want the new path. Actually, for
  // simplicity, let's stick to the standard SDL path. Users can symlink if they
  // really want ~/.config.

  // Ensure directory exists
  std::error_code ec;
  std::filesystem::create_directories(configDir_, ec);
  if (ec) {
    std::fprintf(stderr, "ConfigManager: failed to create dir %s: %s\n",
                 configDir_.c_str(), ec.message().c_str());
    return false;
  }

  configPath_ = configDir_ / "config.json";
  return true;
}

bool ConfigManager::load(AppConfig &config) const {
  if (configPath_.empty())
    return false;

  std::ifstream ifs(configPath_);
  if (!ifs)
    return false;

  auto json = nlohmann::json::parse(ifs, nullptr, false);
  if (json.is_discarded()) {
    std::fprintf(stderr, "ConfigManager: invalid JSON in %s\n",
                 configPath_.c_str());
    return false;
  }

  // Identity
  if (json.contains("identity")) {
    auto &id = json["identity"];
    config.callsign = id.value("callsign", "");
    config.grid = id.value("grid", "");
    config.lat = id.value("lat", 0.0);
    config.lon = id.value("lon", 0.0);
  }

  // Appearance
  if (json.contains("appearance")) {
    auto &ap = json["appearance"];
    std::string hexColor = ap.value("callsign_color", "");
    if (!hexColor.empty()) {
      config.callsignColor = hexToColor(hexColor, config.callsignColor);
    }
    config.theme = ap.value("theme", "default");
    config.mapNightLights = ap.value("map_night_lights", true);
    config.useMetric = ap.value("use_metric", true);
  }

  // Pane widget selection
  if (json.contains("panes")) {
    auto &pa = json["panes"];
    auto loadRotation = [&](const std::string &key,
                            const std::string &legacyKey,
                            std::vector<WidgetType> &vec, WidgetType fallback) {
      if (pa.contains(key) && pa[key].is_array()) {
        vec.clear();
        for (auto &item : pa[key]) {
          if (item.is_string()) {
            vec.push_back(
                widgetTypeFromString(item.get<std::string>(), fallback));
          }
        }
      } else if (pa.contains(legacyKey)) {
        vec = {widgetTypeFromString(pa.value(legacyKey, ""), fallback)};
      }
      if (vec.empty())
        vec = {fallback};
    };

    loadRotation("pane1_rotation", "pane1_widget", config.pane1Rotation,
                 WidgetType::SOLAR);
    loadRotation("pane2_rotation", "pane2_widget", config.pane2Rotation,
                 WidgetType::DX_CLUSTER);
    loadRotation("pane3_rotation", "pane3_widget", config.pane3Rotation,
                 WidgetType::LIVE_SPOTS);
    loadRotation("pane4_rotation", "pane4_widget", config.pane4Rotation,
                 WidgetType::BAND_CONDITIONS);
    config.rotationIntervalS = pa.value("rotation_interval_s", 30);
  }

  // Panel state
  if (json.contains("panel")) {
    auto &pn = json["panel"];
    config.panelMode = pn.value("mode", "dx");
    config.selectedSatellite = pn.value("satellite", "");
  }

  // DX Cluster
  if (json.contains("dx_cluster")) {
    auto &dxc = json["dx_cluster"];
    config.dxClusterEnabled = dxc.value("enabled", true);
    config.dxClusterHost = dxc.value("host", "dxusa.net");
    config.dxClusterPort = dxc.value("port", 7300);
    config.dxClusterLogin = dxc.value("login", "");
    config.dxClusterUseWSJTX = dxc.value("use_wsjtx", false);
  }

  // PSK Reporter
  if (json.contains("psk_reporter")) {
    auto &psk = json["psk_reporter"];
    config.pskOfDe = psk.value("of_de", false);
    config.pskUseCall = psk.value("use_call", false);
    config.pskMaxAge = psk.value("max_age", 30);
    config.pskBands = psk.value("bands_mask", 0xFFF);
  }

  // Power
  if (json.contains("power")) {
    auto &p = json["power"];
    config.preventSleep = p.value("prevent_sleep", true);
  }

  // Require at least a callsign to consider config valid
  return !config.callsign.empty();
}

bool ConfigManager::save(const AppConfig &config) const {
  if (configPath_.empty())
    return false;

  // Create directory if needed
  std::error_code ec;
  std::filesystem::create_directories(configDir_, ec);
  if (ec) {
    std::fprintf(stderr, "ConfigManager: cannot create %s: %s\n",
                 configDir_.c_str(), ec.message().c_str());
    return false;
  }

  nlohmann::json json;
  json["identity"]["callsign"] = config.callsign;
  json["identity"]["grid"] = config.grid;
  json["identity"]["lat"] = config.lat;
  json["identity"]["lon"] = config.lon;

  json["appearance"]["callsign_color"] = colorToHex(config.callsignColor);
  json["appearance"]["theme"] = config.theme;
  json["appearance"]["map_night_lights"] = config.mapNightLights;
  json["appearance"]["use_metric"] = config.useMetric;

  json["power"]["prevent_sleep"] = config.preventSleep;

  auto saveRotation = [&](const std::string &key,
                          const std::vector<WidgetType> &vec) {
    auto arr = nlohmann::json::array();
    for (auto t : vec) {
      arr.push_back(widgetTypeToString(t));
    }
    json["panes"][key] = arr;
  };

  saveRotation("pane1_rotation", config.pane1Rotation);
  saveRotation("pane2_rotation", config.pane2Rotation);
  saveRotation("pane3_rotation", config.pane3Rotation);
  saveRotation("pane4_rotation", config.pane4Rotation);
  json["panes"]["rotation_interval_s"] = config.rotationIntervalS;

  json["panel"]["mode"] = config.panelMode;
  json["panel"]["satellite"] = config.selectedSatellite;

  json["dx_cluster"]["enabled"] = config.dxClusterEnabled;
  json["dx_cluster"]["host"] = config.dxClusterHost;
  json["dx_cluster"]["port"] = config.dxClusterPort;
  json["dx_cluster"]["login"] = config.dxClusterLogin;
  json["dx_cluster"]["use_wsjtx"] = config.dxClusterUseWSJTX;

  json["psk_reporter"]["of_de"] = config.pskOfDe;
  json["psk_reporter"]["use_call"] = config.pskUseCall;
  json["psk_reporter"]["max_age"] = config.pskMaxAge;
  json["psk_reporter"]["bands_mask"] = config.pskBands;

  std::ofstream ofs(configPath_);
  if (!ofs) {
    std::fprintf(stderr, "ConfigManager: cannot write %s\n",
                 configPath_.c_str());
    return false;
  }

  ofs << json.dump(2) << "\n";
  return ofs.good();
}
