#include "ConfigManager.h"
#include "EEPROMMigrator.h"
#include "Logger.h"

#include <SDL.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static std::string propOverlayToStr(PropOverlayType t) {
  switch (t) {
  case PropOverlayType::Muf:         return "muf";
  case PropOverlayType::Voacap:      return "voacap";
  case PropOverlayType::Reliability: return "reliability";
  case PropOverlayType::Toa:         return "toa";
  case PropOverlayType::Heatmap:     return "heatmap";
  case PropOverlayType::Drap:        return "drap";
  case PropOverlayType::Aurora:      return "aurora";
  default:                           return "none";
  }
}

static PropOverlayType propOverlayFromStr(const std::string &s) {
  if (s == "muf")         return PropOverlayType::Muf;
  if (s == "voacap")      return PropOverlayType::Voacap;
  if (s == "reliability") return PropOverlayType::Reliability;
  if (s == "toa")         return PropOverlayType::Toa;
  if (s == "heatmap")     return PropOverlayType::Heatmap;
  if (s == "drap")        return PropOverlayType::Drap;
  if (s == "aurora")      return PropOverlayType::Aurora;
  return PropOverlayType::None;
}

static std::string weatherOverlayToStr(WeatherOverlayType t) {
  if (t == WeatherOverlayType::WxMb)       return "wxmb";
  if (t == WeatherOverlayType::CloudsGrib) return "clouds_grib";
  return "none";
}

static WeatherOverlayType weatherOverlayFromStr(const std::string &s) {
  if (s == "wxmb")        return WeatherOverlayType::WxMb;
  if (s == "clouds_grib" || s == "clouds") return WeatherOverlayType::CloudsGrib;
  return WeatherOverlayType::None;
}

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
  // On Windows: %APPDATA%\HamClock\HamClock-Next
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
                 configDir_.string().c_str(), ec.message().c_str());
    return false;
  }

#ifdef __EMSCRIPTEN__
  // Mount IDBFS and sync (load from IndexedDB into virtual FS).
  // IMPORTANT: SDL_GetPrefPath returns a path with a trailing slash.
  // FS.mount() requires a clean path with no trailing slash or it silently
  // fails, causing every save to write to session-only MEMFS that vanishes
  // on reload.  Strip the trailing slash before passing to JS.
  std::string mountPath = configDir_.string();
  while (mountPath.size() > 1 && mountPath.back() == '/')
    mountPath.pop_back();

  EM_ASM(
      {
        var path = UTF8ToString($0);
        console.log('[IDBFS] Mounting at: ' + path);

        // Create the mount point directory in MEMFS if needed.
        // Parent dirs already exist (created by create_directories above).
        try {
          FS.mkdir(path);
        } catch (e) {
          // EEXIST is expected on every run after the first — not an error.
        }

        var mounted = false;
        try {
          FS.mount(IDBFS, {}, path);
          mounted = true;
          console.log('[IDBFS] Mount succeeded');
        } catch (e) {
          console.error('[IDBFS] Mount FAILED — config will not persist:', e);
        }

        // Sync from IndexedDB into MEMFS, then notify C++.
        // If mount failed we still call back so the app is not stuck in
        // Loading state; it will just show the setup screen every run.
        FS.syncfs(
            true, function(err) {
              if (err) {
                console.error('[IDBFS] Sync-from-IDB failed:', err);
              } else {
                console.log('[IDBFS] Sync-from-IDB complete' +
                            (mounted ? "" : " (no IDBFS — session only)"));
              }
              if (typeof Module._hamclock_after_idbfs === 'function')
                Module._hamclock_after_idbfs();
            });
      },
      mountPath.c_str());
#endif

  configPath_ = configDir_ / "config.json";
  return true;
}

static void ensureFactoryPresets(AppConfig &config) {
  // Factory preset names (must match the names defined below)
  static const std::set<std::string> factoryNames = {
      "DX", "Contest", "Satellite", "Environment/WX", "Prop Firehose", "DE Station Status"};

  // Remove any user presets that shadow factory presets (unless explicitly deleted)
  auto it = config.presets.begin();
  while (it != config.presets.end()) {
    if (factoryNames.count(it->name) && config.deletedFactoryPresets.find(it->name) == config.deletedFactoryPresets.end()) {
      it = config.presets.erase(it);
    } else {
      ++it;
    }
  }

  auto presetExists = [&](const std::string &name) {
    return std::any_of(config.presets.begin(), config.presets.end(),
                       [&](const ConfigPreset &p) { return p.name == name; });
  };

  auto addIfMissing = [&](ConfigPreset p) {
    if (!presetExists(p.name) && config.deletedFactoryPresets.find(p.name) == config.deletedFactoryPresets.end()) {
      config.presets.push_back(std::move(p));
    }
  };

  // DX preset
  {
    ConfigPreset p;
    p.name = "DX";
    p.pane1Rotation = {"dx_cluster"};
    p.pane2Rotation = {"dx_peditions"};
    p.pane3Rotation = {"live_spots"};
    p.pane4Rotation = {"band_conditions"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"dx_info"};
    p.propOverlay = PropOverlayType::Reliability;
    addIfMissing(std::move(p));
  }
  // Contest preset
  {
    ConfigPreset p;
    p.name = "Contest";
    p.pane1Rotation = {"dx_cluster"};
    p.pane2Rotation = {"live_spots"};
    p.pane3Rotation = {"band_conditions"};
    p.pane4Rotation = {"solar"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"dx_info"};
    p.propOverlay = PropOverlayType::Heatmap;
    addIfMissing(std::move(p));
  }
  // Satellite preset
  {
    ConfigPreset p;
    p.name = "Satellite";
    p.pane1Rotation = {"moon"};
    p.pane2Rotation = {"eme_tool"};
    p.pane3Rotation = {"gimbal"};
    p.pane4Rotation = {"solar"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"satellite"};
    addIfMissing(std::move(p));
  }
  // Environment/WX preset
  {
    ConfigPreset p;
    p.name = "Environment/WX";
    p.pane1Rotation = {"de_weather"};
    p.pane2Rotation = {"dx_weather"};
    p.pane3Rotation = {"forecast"};
    p.pane4Rotation = {"solar"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"dx_info"};
    p.weatherOverlay = WeatherOverlayType::WxMb;
    addIfMissing(std::move(p));
  }
  // Propagation Firehose preset
  {
    ConfigPreset p;
    p.name = "Prop Firehose";
    p.pane1Rotation = {"solar", "solar_storm", "solar_cycle", "ionosonde"};
    p.pane2Rotation = {"solar_timeline", "sfi_trend", "noaa_spacewx", "tropo"};
    p.pane3Rotation = {"aurora", "aurora_graph", "voacap_dedx", "drap"};
    p.pane4Rotation = {"band_conditions", "ncdxf"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"dx_info"};
    p.propRotation = {PropOverlayType::Muf, PropOverlayType::Reliability,
                      PropOverlayType::Heatmap, PropOverlayType::Drap,
                      PropOverlayType::Aurora};
    p.propOverlay = PropOverlayType::Muf;
    p.rotationIntervalS = 30;
    addIfMissing(std::move(p));
  }
  // DE Station Status preset
  {
    ConfigPreset p;
    p.name = "DE Station Status";
    p.pane1Rotation = {"dxcc_progress", "was_progress", "grid_progress"};
    p.pane2Rotation = {"zone_heatmap", "clublog_wanted"};
    p.pane3Rotation = {"wac_radar", "alerts"};
    p.pane4Rotation = {"band_conditions"};
    p.pane5Rotation = {"de_info"};
    p.pane6Rotation = {"lotw_sync", "adif"};
    p.weatherOverlay = WeatherOverlayType::CloudsGrib;
    addIfMissing(std::move(p));
  }
}

bool ConfigManager::load(AppConfig &config) {
  if (configPath_.empty())
    return false;

  bool loadedJson = false;
  std::ifstream ifs(configPath_);
  if (ifs) {
    auto json = nlohmann::json::parse(ifs, nullptr, false);
    if (!json.is_discarded()) {
      loadedJson = true;
      // ... (existing parsing logic)

  // Identity
  if (json.contains("identity")) {
    auto &id = json["identity"];
    config.callsign = id.value("callsign", "");
    config.grid = id.value("grid", "");
    config.lat = id.value("lat", 0.0);
    config.lon = id.value("lon", 0.0);
    config.defaultTzOffset = id.value("default_tz_offset", 0);
    config.defaultTzLabel = id.value("default_tz_label", "UTC");
  }

  // Appearance
  if (json.contains("appearance")) {
    auto &ap = json["appearance"];
    std::string hexColor = ap.value("callsign_color", "");
    if (!hexColor.empty()) {
      config.callsignColor = hexToColor(hexColor, config.callsignColor);
    }
    std::string hexBgColor = ap.value("callsign_bg_color", "");
    if (!hexBgColor.empty()) {
      config.callsignBgColor = hexToColor(hexBgColor, {0, 0, 0, 0});
    }
    config.theme = ap.value("theme", "default");
    config.propColormap = ap.value("prop_colormap", "muted");
    config.mapNightLights = ap.value("map_night_lights", true);
    config.useMetric = ap.value("use_metric", true);
    config.projection = ap.value("projection", "equirectangular");
    config.mapStyle = ap.value("map_style", "nasa");
    if (config.mapStyle.empty()) config.mapStyle = "nasa";
    config.gridType = ap.value("grid_type", "latlon");
    config.centerMapOnDe = ap.value("center_map_on_de", false);

    // Legacy migration: if show_muf_rt is present and true, defaulting to Muf
    if (ap.contains("prop_overlay")) {
      config.propOverlay = propOverlayFromStr(ap.value("prop_overlay", "none"));
    } else if (ap.contains("show_muf_rt")) {
      bool showMuf = ap.value("show_muf_rt", false);
      config.propOverlay =
          showMuf ? PropOverlayType::Muf : PropOverlayType::None;
    }

    if (ap.contains("prop_rotation") && ap["prop_rotation"].is_array()) {
      config.propRotation.clear();
      for (auto &item : ap["prop_rotation"]) {
        if (item.is_string())
          config.propRotation.push_back(propOverlayFromStr(item.get<std::string>()));
      }
    }

    if (ap.contains("weather_overlay")) {
      config.weatherOverlay =
          weatherOverlayFromStr(ap.value("weather_overlay", "none"));
    }

    config.propBand = ap.value("prop_band", "20m");
    config.propMode = ap.value("prop_mode", "SSB");
    config.propPower = ap.value("prop_power", 100);
    config.propToa = ap.value("prop_toa", 3.0f);
    config.propPath = ap.value("prop_path", 0);
    config.propAntGain = ap.value("prop_ant_gain", 3);
    config.mufRtOpacity = ap.value("muf_rt_opacity", 40);
    config.showSatTrack = ap.value("show_sat_track", true);
    config.showBeacons = ap.value("show_beacons", true);
    config.showBorders = ap.value("show_borders", false);
    config.displayPowerMethod = ap.value("display_power_method", "auto");
    config.logLevel = ap.value("log_level", "warn");
    config.qrzUsername = ap.value("qrz_username", "");
    config.qrzPassword = ap.value("qrz_password", "");
    config.fontPath = ap.value("font_path", "");
  }

  // Countdown (new dedicated section; falls back to legacy appearance keys)
  if (json.contains("countdown")) {
    auto &cd = json["countdown"];
    config.countdownLabel = cd.value("label", "");
    config.countdownTime = cd.value("time", "");
  } else if (json.contains("appearance")) {
    auto &ap = json["appearance"];
    config.countdownLabel = ap.value("countdown_label", "");
    config.countdownTime = ap.value("countdown_time", "");
  }

  // Reminder
  if (json.contains("reminder")) {
    auto &rm = json["reminder"];
    config.callsignExpiry = rm.value("callsign_expiry", "");
    config.callsignFrn = rm.value("callsign_frn", "");
    config.callsignExpiryAcknowledged = rm.value("callsign_expiry_acked", "");
    config.callsignExpiryLastCheck =
        rm.value("callsign_expiry_last_check", (int64_t)0);
    if (rm.contains("custom") && rm["custom"].is_array()) {
      config.reminders.clear();
      for (auto &item : rm["custom"]) {
        ReminderEntry re;
        re.label = item.value("label", "");
        re.date = item.value("date", "");
        re.active = item.value("active", true);
        re.acknowledgedDate = item.value("acked", "");
        config.reminders.push_back(re);
      }
    }
  }

  // Aux Clock
  if (json.contains("aux_clock")) {
    config.auxClockTzOffset  = json["aux_clock"].value("tz_offset", 0);
    config.auxClockTzLabel   = json["aux_clock"].value("tz_label", std::string("UTC"));
    config.auxClockStarMode  = json["aux_clock"].value("star_mode", 1);
  }

  // Big Clock
  if (json.contains("big_clock")) {
    auto &bc = json["big_clock"];
    config.bigClockDigital  = bc.value("digital",  true);
    config.bigClock12h = bc.value("12h", false);
    config.bigClockUtc = bc.value("utc", false);
    config.bigClockUseDefaultTz = bc.value("use_default_tz", false);
    config.bigClockShowSec = bc.value("show_sec", true);
    config.bigClockShowDate = bc.value("show_date", true);
    config.bigClockHue      = static_cast<uint8_t>(bc.value("hue", 85));
  }

  // RSS
  if (json.contains("rss")) {
    config.rssEnabled = json["rss"].value("enabled", true);
    config.rssUrl = json["rss"].value("url", "");
  }

  // Activity panels
  if (json.contains("activity")) {
    config.ontaFilter = json["activity"].value("onta_filter", "all");
    config.ontaMaxDistKm = json["activity"].value("onta_max_dist_km", 0);
  }

  // Calendar notifications
  if (json.contains("calendar")) {
    config.calendarNotifyMinutes =
        json["calendar"].value("notify_minutes", 10);
    config.calendarAllDayNotifyHour =
        json["calendar"].value("allday_notify_hour", 8);
    config.calendarDismissMinutes =
        json["calendar"].value("dismiss_minutes", 10);
  }

  if (json.contains("asteroid")) {
    config.asteroidIcon = json["asteroid"].value("icon", std::string("☄"));
    if (json["asteroid"].contains("color")) {
      auto &c = json["asteroid"]["color"];
      config.asteroidColor.r = c.value("r", uint8_t(255));
      config.asteroidColor.g = c.value("g", uint8_t(140));
      config.asteroidColor.b = c.value("b", uint8_t(0));
      config.asteroidColor.a = 255;
    }
  }

  // Network (WASM proxy URL)
  if (json.contains("network")) {
    const auto &n = json["network"];
    config.corsProxyUrl = n.value("cors_proxy_url", config.corsProxyUrl);
  }

    // Color Overrides
  if (json.contains("color_overrides") && json["color_overrides"].is_object()) {
    for (auto &el : json["color_overrides"].items()) {
      if (el.value().is_string()) {
        config.colorOverrides[el.key()] =
            hexToColor(el.value().get<std::string>(), {0, 0, 0, 255});
      }
    }
  }

  // Ensure default custom propagation colors exist
  if (config.colorOverrides.find("prop_color_0") == config.colorOverrides.end())
    config.colorOverrides["prop_color_0"] = {150, 0, 0, 255};
  if (config.colorOverrides.find("prop_color_25") == config.colorOverrides.end())
    config.colorOverrides["prop_color_25"] = {255, 150, 0, 255};
  if (config.colorOverrides.find("prop_color_50") == config.colorOverrides.end())
    config.colorOverrides["prop_color_50"] = {255, 255, 0, 255};
  if (config.colorOverrides.find("prop_color_75") == config.colorOverrides.end())
    config.colorOverrides["prop_color_75"] = {0, 255, 255, 255};
  if (config.colorOverrides.find("prop_color_100") == config.colorOverrides.end())
    config.colorOverrides["prop_color_100"] = {255, 255, 255, 255};

  // Local Data Hub
  if (json.contains("hub")) {
    const auto &h = json["hub"];
    std::string mode = h.value("mode", "off");
    if (mode == "master")
      config.hubMode = HubMode::Master;
    else if (mode == "client")
      config.hubMode = HubMode::Client;
    else
      config.hubMode = HubMode::Off;
    config.hubIp = h.value("ip", "");
    config.hubPort = h.value("port", 8080);
  }

  // Brightness
  if (json.contains("brightness")) {
    auto &br = json["brightness"];
    config.brightness = br.value("level", 100);
    config.brightnessSchedule = br.value("schedule", false);
    config.dimHour = br.value("dim_hour", 22);
    config.dimMinute = br.value("dim_minute", 0);
    config.brightHour = br.value("bright_hour", 6);
    config.brightMinute = br.value("bright_minute", 0);
    config.ltr329AutoDim = br.value("ltr329_auto_dim", false);
  }

  // Pane widget selection
  if (json.contains("panes")) {
    auto &pa = json["panes"];
    auto loadRotation = [&](const std::string &key,
                            const std::string &legacyKey,
                            std::vector<std::string> &vec,
                            const std::string &fallback,
                            bool allowEmpty = false) {
      if (pa.contains(key) && pa[key].is_array()) {
        vec.clear();
        for (auto &item : pa[key]) {
          if (item.is_string())
            vec.push_back(item.get<std::string>());
        }
      } else if (pa.contains(legacyKey)) {
        std::string v = pa.value(legacyKey, "");
        if (!v.empty())
          vec = {v};
      }
      if (vec.empty() && !allowEmpty)
        vec = {fallback};
    };

    loadRotation("pane1_rotation", "pane1_widget", config.pane1Rotation, "solar");
    loadRotation("pane2_rotation", "pane2_widget", config.pane2Rotation, "dx_cluster");
    loadRotation("pane3_rotation", "pane3_widget", config.pane3Rotation, "live_spots");
    loadRotation("pane4_rotation", "pane4_widget", config.pane4Rotation, "band_conditions");
    loadRotation("pane5_rotation", "pane5_widget", config.pane5Rotation, "de_info");
    loadRotation("pane6_rotation", "pane6_widget", config.pane6Rotation, "dx_info", /*allowEmpty=*/true);
    
    if (pa.contains("prop_rotation") && pa["prop_rotation"].is_array()) {
      config.propRotation.clear();
      for (auto &item : pa["prop_rotation"]) {
        if (item.is_string())
          config.propRotation.push_back(propOverlayFromStr(item.get<std::string>()));
      }
    }

    config.rotationIntervalS = pa.value("rotation_interval_s", 30);
    config.syncRotation = pa.value("sync_rotation", false);
    if (pa.contains("watchlist") && pa["watchlist"].is_array()) {
      for (const auto &e : pa["watchlist"]) {
        if (e.is_string())
          config.watchlist.push_back(e.get<std::string>());
      }
    }
  }

  // Panel state
  if (json.contains("panel")) {
    auto &pn = json["panel"];
    config.panelMode = pn.value("mode", "dx");
    config.selectedSatellite = pn.value("satellite", "");
    config.satWidgetSatellite = pn.value("sat_widget_satellite", "");
    if (pn.contains("custom_sccs") && pn["custom_sccs"].is_array()) {
      config.customSatelliteSCCs.clear();
      for (auto &item : pn["custom_sccs"]) {
        if (item.is_number())
          config.customSatelliteSCCs.push_back(item.get<int>());
      }
    }
  }

  // DX Cluster
  if (json.contains("dx_cluster")) {
    auto &dxc = json["dx_cluster"];
    config.dxClusterEnabled = dxc.value("enabled", true);
    config.dxClusterHost = dxc.value("host", "dxusa.net");
    config.dxClusterPort = dxc.value("port", 7300);
    config.dxClusterLogin = dxc.value("login", "");
    config.dxClusterUseWSJTX = dxc.value("use_wsjtx", false);
    config.wsjtxPort = dxc.value("wsjtx_port", 2237);
    config.dxClusterHideDuplicates = dxc.value("hide_duplicates", true);
    config.dxClusterMaxAgeMinutes = dxc.value("max_age_minutes", 20);
  }

  // Live Spots (Combined RBN, PSK Reporter, WSPR)
  if (json.contains("live_spots")) {
    auto &ls = json["live_spots"];
    std::string src = ls.value("source", "psk");
    if (src == "rbn")
      config.liveSpotSource = LiveSpotSource::RBN;
    else if (src == "wspr")
      config.liveSpotSource = LiveSpotSource::WSPR;
    else
      config.liveSpotSource = LiveSpotSource::PSK;

    config.liveSpotsOfDe = ls.value("of_de", true);
    config.liveSpotsUseCall = ls.value("use_call", true);
    config.liveSpotsMaxAge = ls.value("max_age", 30);
    config.liveSpotsBands = ls.value("bands_mask", 0xFFF);
    config.rbnHost = ls.value("rbn_host", "telnet.reversebeacon.net");
    config.rbnPort = ls.value("rbn_port", 7000);
  } else {
    // Migration from legacy sections
    if (json.contains("rbn")) {
      auto &rbn = json["rbn"];
      if (rbn.value("enabled", false)) {
        config.liveSpotSource = LiveSpotSource::RBN;
      }
      config.rbnHost = rbn.value("host", "telnet.reversebeacon.net");
    }
    if (json.contains("psk_reporter")) {
      auto &psk = json["psk_reporter"];
      config.liveSpotsOfDe = psk.value("of_de", true);
      config.liveSpotsUseCall = psk.value("use_call", true);
      config.liveSpotsMaxAge = psk.value("max_age", 30);
      config.liveSpotsBands = psk.value("bands_mask", 0xFFF);
    }
  }

  // SDO Settings
  if (json.contains("sdo")) {
    auto &s = json["sdo"];
    config.sdoWavelength = s.value("wavelength", "0193");
    config.sdoRotating = s.value("rotating", false);
    config.sdoPfss = s.value("pfss", s.value("grayline", false));
    config.sdoShowMovie = s.value("show_movie", false);
  }

  // Marine
  if (json.contains("marine")) {
    auto &m = json["marine"];
    config.marineStation = m.value("station", "");
    config.marineBuoy = m.value("buoy", "");
  }

  // Power
  if (json.contains("power")) {
    auto &p = json["power"];
    config.preventSleep = p.value("prevent_sleep", true);
    config.gpsEnabled = p.value("gps_enabled", false);
    config.audioMuted = p.value("audio_muted", false);
    config.audioVolume = p.value("audio_volume", 100);
    config.skippedVersion = p.value("skipped_version", "");
  }

  // Rotator (Hamlib rotctld)
  // Rotator
  if (json.contains("rotator")) {
    const auto &r = json["rotator"];
    config.rotatorHost = r.value("host", "");
    config.rotatorPort = r.value("port", 4533);
    config.rotatorAutoTrack = r.value("auto_track", false);
    config.rotatorUpover = r.value("upover", false);
  }
  // Rig (Hamlib rigctld)
  // Rig
  if (json.contains("rig")) {
    auto &r = json["rig"];
    config.rigHost = r.value("host", "");
    config.rigPort = r.value("port", 4532);
    config.rigAutoTune = r.value("auto_tune", true);
  }

  // API Keys
  if (json.contains("api_keys")) {
    auto &k = json["api_keys"];
    config.repeaterBookKey = k.value("repeaterbook", "");
    config.winlinkKey = k.value("winlink", "");
  }
  // Presets
  if (json.contains("presets") && json["presets"].is_array()) {
    config.presets.clear();
    auto loadPresetRotation = [](const nlohmann::json &jp,
                                 const std::string &key,
                                 std::vector<std::string> &vec) {
      if (jp.contains(key) && jp[key].is_array())
        for (auto &e : jp[key])
          if (e.is_string())
            vec.push_back(e.get<std::string>());
    };
    for (auto &jp : json["presets"]) {
      ConfigPreset p;
      p.name = jp.value("name", "");
      loadPresetRotation(jp, "pane1_rotation", p.pane1Rotation);
      loadPresetRotation(jp, "pane2_rotation", p.pane2Rotation);
      loadPresetRotation(jp, "pane3_rotation", p.pane3Rotation);
      loadPresetRotation(jp, "pane4_rotation", p.pane4Rotation);
      loadPresetRotation(jp, "pane5_rotation", p.pane5Rotation);
      loadPresetRotation(jp, "pane6_rotation", p.pane6Rotation);
      p.rotationIntervalS = jp.value("rotation_interval_s", 30);
      p.propOverlay  = propOverlayFromStr(jp.value("prop_overlay", "none"));
      if (jp.contains("prop_rotation") && jp["prop_rotation"].is_array()) {
        for (auto &e : jp["prop_rotation"])
          if (e.is_string())
            p.propRotation.push_back(propOverlayFromStr(e.get<std::string>()));
      }
      p.weatherOverlay = weatherOverlayFromStr(jp.value("weather_overlay", "none"));
      p.mapStyle     = jp.value("map_style", "nasa");
      p.mapNightLights = jp.value("map_night_lights", true);
      p.showGrid     = jp.value("show_grid", false);
      p.gridType     = jp.value("grid_type", "latlon");
      p.propBand     = jp.value("prop_band", "20m");
      p.propMode     = jp.value("prop_mode", "SSB");
      p.propPower    = jp.value("prop_power", 100);
      p.propToa      = jp.value("prop_toa", 3.0f);
      p.propPath     = jp.value("prop_path", 0);
      p.propAntGain  = jp.value("prop_ant_gain", 3);
      config.presets.push_back(std::move(p));
    }
  }

  // Deleted factory presets
  if (json.contains("deleted_factory_presets") && json["deleted_factory_presets"].is_array()) {
    for (auto &item : json["deleted_factory_presets"]) {
      if (item.is_string())
        config.deletedFactoryPresets.insert(item.get<std::string>());
    }
  }

  // World Clock
  config.worldClocks.clear();
  if (json.contains("world_clocks") && json["world_clocks"].is_array()) {
    for (auto &item : json["world_clocks"]) {
      WorldClockEntry entry;
      entry.label = item.value("label", "");
      entry.offsetMinutes = item.value("offset_minutes", 0);
      entry.active = item.value("active", false);
      config.worldClocks.push_back(entry);
    }
  }
}
}

if (!loadedJson) {
  if (!EEPROMMigrator::migrate(config)) {
    return false;
  }
  save(config); // Persist migrated settings immediately
}
  // Ensure we always have 4 entries
  while (config.worldClocks.size() < 4) {
    config.worldClocks.push_back({"", 0, false});
  }

  // Ensure factory presets exist (skip deleted ones)
  ensureFactoryPresets(config);

  // Sync internal state
  config_ = config;

  // Require at least a callsign to consider config valid
  return !config.callsign.empty();
}

bool ConfigManager::save(const AppConfig &config) {
  if (configPath_.empty())
    return false;

  // Sync internal state
  config_ = config;

  // Create directory if needed
  std::error_code ec;
  std::filesystem::create_directories(configDir_, ec);
  if (ec) {
    std::fprintf(stderr, "ConfigManager: cannot create %s: %s\n",
                 configDir_.string().c_str(), ec.message().c_str());
    return false;
  }

  nlohmann::json json;
  json["identity"]["callsign"] = config.callsign;
  json["identity"]["grid"] = config.grid;
  json["identity"]["lat"] = config.lat;
  json["identity"]["lon"] = config.lon;
  json["identity"]["default_tz_offset"] = config.defaultTzOffset;
  json["identity"]["default_tz_label"] = config.defaultTzLabel;

  json["appearance"]["callsign_color"] = colorToHex(config.callsignColor);
  if (config.callsignBgColor.a > 0)
    json["appearance"]["callsign_bg_color"] = colorToHex(config.callsignBgColor);
  json["appearance"]["theme"] = config.theme;
  json["appearance"]["prop_colormap"] = config.propColormap;
  json["appearance"]["map_night_lights"] = config.mapNightLights;
  json["appearance"]["use_metric"] = config.useMetric;
  json["appearance"]["projection"] = config.projection;
  json["appearance"]["map_style"] = config.mapStyle;
  json["appearance"]["grid_type"] = config.gridType;
  json["appearance"]["center_map_on_de"] = config.centerMapOnDe;
  json["appearance"]["prop_overlay"] = propOverlayToStr(config.propOverlay);
  {
    auto arr = nlohmann::json::array();
    for (auto t : config.propRotation)
      arr.push_back(propOverlayToStr(t));
    json["appearance"]["prop_rotation"] = arr;
  }
  json["appearance"]["weather_overlay"] = weatherOverlayToStr(config.weatherOverlay);
  json["appearance"]["prop_band"] = config.propBand;
  json["appearance"]["prop_mode"] = config.propMode;
  json["appearance"]["prop_power"] = config.propPower;
  json["appearance"]["prop_toa"] = config.propToa;
  json["appearance"]["prop_path"] = config.propPath;
  json["appearance"]["prop_ant_gain"] = config.propAntGain;
  // Legacy compat
  json["appearance"]["show_muf_rt"] =
      (config.propOverlay == PropOverlayType::Muf);
  json["appearance"]["muf_rt_opacity"] = config.mufRtOpacity;
  json["appearance"]["show_sat_track"] = config.showSatTrack;
  json["appearance"]["show_beacons"] = config.showBeacons;
  json["appearance"]["show_borders"] = config.showBorders;
  json["appearance"]["display_power_method"] = config.displayPowerMethod;
  if (config.logLevel != "warn")
    json["appearance"]["log_level"] = config.logLevel;
  json["appearance"]["qrz_username"] = config.qrzUsername;
  json["appearance"]["qrz_password"] = config.qrzPassword;
  if (!config.fontPath.empty())
    json["appearance"]["font_path"] = config.fontPath;
  json["countdown"]["label"] = config.countdownLabel;
  json["countdown"]["time"] = config.countdownTime;

  json["reminder"]["callsign_expiry"] = config.callsignExpiry;
  json["reminder"]["callsign_frn"] = config.callsignFrn;
  json["reminder"]["callsign_expiry_acked"] = config.callsignExpiryAcknowledged;
  json["reminder"]["callsign_expiry_last_check"] =
      config.callsignExpiryLastCheck;
  auto rmArr = nlohmann::json::array();
  for (const auto &re : config.reminders) {
    nlohmann::json jre;
    jre["label"] = re.label;
    jre["date"] = re.date;
    jre["active"] = re.active;
    jre["acked"] = re.acknowledgedDate;
    rmArr.push_back(jre);
  }
  json["reminder"]["custom"] = rmArr;

  json["brightness"]["level"] = config.brightness;
  json["brightness"]["schedule"] = config.brightnessSchedule;
  json["brightness"]["dim_hour"] = config.dimHour;
  json["brightness"]["dim_minute"] = config.dimMinute;
  json["brightness"]["bright_hour"] = config.brightHour;
  json["brightness"]["bright_minute"] = config.brightMinute;
  json["brightness"]["ltr329_auto_dim"] = config.ltr329AutoDim;

  json["power"]["prevent_sleep"] = config.preventSleep;
  json["power"]["gps_enabled"] = config.gpsEnabled;
  json["power"]["audio_muted"] = config.audioMuted;
  json["power"]["audio_volume"] = config.audioVolume;
  json["power"]["skipped_version"] = config.skippedVersion;

  json["network"]["cors_proxy_url"] = config.corsProxyUrl;

  json["hub"]["mode"] = (config.hubMode == HubMode::Master)   ? "master"
                        : (config.hubMode == HubMode::Client) ? "client"
                                                              : "off";
  json["hub"]["ip"] = config.hubIp;
  json["hub"]["port"] = config.hubPort;

  json["sdo"]["wavelength"] = config.sdoWavelength;
  json["sdo"]["rotating"] = config.sdoRotating;
  json["sdo"]["pfss"] = config.sdoPfss;
  json["sdo"]["show_movie"] = config.sdoShowMovie;

  json["marine"]["station"] = config.marineStation;
  json["marine"]["buoy"] = config.marineBuoy;

  json["rotator"]["host"] = config.rotatorHost;
  json["rotator"]["port"] = config.rotatorPort;
  json["rotator"]["auto_track"] = config.rotatorAutoTrack;
  json["rotator"]["upover"] = config.rotatorUpover;

  json["rig"]["host"] = config.rigHost;
  json["rig"]["port"] = config.rigPort;
  json["rig"]["auto_tune"] = config.rigAutoTune;

  json["api_keys"]["repeaterbook"] = config.repeaterBookKey;
  json["api_keys"]["winlink"] = config.winlinkKey;

  auto saveRotation = [&](const std::string &key,
                          const std::vector<std::string> &vec) {
    auto arr = nlohmann::json::array();
    for (const auto &t : vec)
      arr.push_back(t);
    json["panes"][key] = arr;
  };

  saveRotation("pane1_rotation", config.pane1Rotation);
  saveRotation("pane2_rotation", config.pane2Rotation);
  saveRotation("pane3_rotation", config.pane3Rotation);
  saveRotation("pane4_rotation", config.pane4Rotation);
  saveRotation("pane5_rotation", config.pane5Rotation);
  saveRotation("pane6_rotation", config.pane6Rotation);
  json["panes"]["rotation_interval_s"] = config.rotationIntervalS;
  {
    auto arr = nlohmann::json::array();
    for (auto t : config.propRotation)
      arr.push_back(propOverlayToStr(t));
    json["panes"]["prop_rotation"] = arr;
  }
  json["panes"]["sync_rotation"] = config.syncRotation;
  {
    auto wl = nlohmann::json::array();
    for (const auto &c : config.watchlist)
      wl.push_back(c);
    json["panes"]["watchlist"] = wl;
  }

  json["panel"]["mode"] = config.panelMode;
  json["panel"]["satellite"] = config.selectedSatellite;
  json["panel"]["sat_widget_satellite"] = config.satWidgetSatellite;

  auto sccArr = nlohmann::json::array();
  for (int scc : config.customSatelliteSCCs) {
    sccArr.push_back(scc);
  }
  json["panel"]["custom_sccs"] = sccArr;

  json["dx_cluster"]["enabled"] = config.dxClusterEnabled;
  json["dx_cluster"]["host"] = config.dxClusterHost;
  json["dx_cluster"]["port"] = config.dxClusterPort;
  json["dx_cluster"]["login"] = config.dxClusterLogin;
  json["dx_cluster"]["use_wsjtx"] = config.dxClusterUseWSJTX;
  json["dx_cluster"]["wsjtx_port"] = config.wsjtxPort;
  json["dx_cluster"]["hide_duplicates"] = config.dxClusterHideDuplicates;
  json["dx_cluster"]["max_age_minutes"] = config.dxClusterMaxAgeMinutes;

  json["live_spots"]["source"] =
      (config.liveSpotSource == LiveSpotSource::RBN)    ? "rbn"
      : (config.liveSpotSource == LiveSpotSource::WSPR) ? "wspr"
                                                        : "psk";
  json["live_spots"]["of_de"] = config.liveSpotsOfDe;
  json["live_spots"]["use_call"] = config.liveSpotsUseCall;
  json["live_spots"]["max_age"] = config.liveSpotsMaxAge;
  json["live_spots"]["bands_mask"] = config.liveSpotsBands;
  json["live_spots"]["rbn_host"] = config.rbnHost;
  json["live_spots"]["rbn_port"] = config.rbnPort;

  json["aux_clock"]["tz_offset"]  = config.auxClockTzOffset;
  json["aux_clock"]["tz_label"]   = config.auxClockTzLabel;
  json["aux_clock"]["star_mode"]  = config.auxClockStarMode;

  json["big_clock"]["digital"]   = config.bigClockDigital;
  json["big_clock"]["12h"] = config.bigClock12h;
  json["big_clock"]["utc"] = config.bigClockUtc;
  json["big_clock"]["use_default_tz"] = config.bigClockUseDefaultTz;
  json["big_clock"]["show_sec"] = config.bigClockShowSec;
  json["big_clock"]["show_date"] = config.bigClockShowDate;
  json["big_clock"]["hue"]       = config.bigClockHue;
  
  // World Clock
  auto wcArr = nlohmann::json::array();
  for (const auto &entry : config.worldClocks) {
    nlohmann::json item;
    item["label"] = entry.label;
    item["offset_minutes"] = entry.offsetMinutes;
    item["active"] = entry.active;
    wcArr.push_back(item);
  }
  json["world_clocks"] = wcArr;

  json["rss"]["enabled"] = config.rssEnabled;
  json["rss"]["url"] = config.rssUrl;
  json["activity"]["onta_filter"] = config.ontaFilter;
  json["activity"]["onta_max_dist_km"] = config.ontaMaxDistKm;

  json["calendar"]["notify_minutes"] = config.calendarNotifyMinutes;
  json["calendar"]["allday_notify_hour"] = config.calendarAllDayNotifyHour;
  json["calendar"]["dismiss_minutes"] = config.calendarDismissMinutes;
  json["asteroid"]["icon"] = config.asteroidIcon;
  json["asteroid"]["color"]["r"] = config.asteroidColor.r;
  json["asteroid"]["color"]["g"] = config.asteroidColor.g;
  json["asteroid"]["color"]["b"] = config.asteroidColor.b;

  // Color Overrides
  if (!config.colorOverrides.empty()) {
    for (auto const &[key, val] : config.colorOverrides) {
      json["color_overrides"][key] = colorToHex(val);
    }
  }

  // Presets (only save user-created presets, not factory presets)
  {
    static const std::set<std::string> factoryNames = {
        "DX", "Contest", "Satellite", "Environment/WX", "Prop Firehose", "DE Station Status"};

    auto savePresetRotation = [](const std::vector<std::string> &vec) {
      auto arr = nlohmann::json::array();
      for (const auto &t : vec) arr.push_back(t);
      return arr;
    };
    auto presetsArr = nlohmann::json::array();
    for (const auto &p : config.presets) {
      // Skip factory presets; only save user-created ones
      if (factoryNames.count(p.name) > 0)
        continue;

      nlohmann::json jp;
      jp["name"]               = p.name;
      jp["pane1_rotation"]     = savePresetRotation(p.pane1Rotation);
      jp["pane2_rotation"]     = savePresetRotation(p.pane2Rotation);
      jp["pane3_rotation"]     = savePresetRotation(p.pane3Rotation);
      jp["pane4_rotation"]     = savePresetRotation(p.pane4Rotation);
      jp["pane5_rotation"]     = savePresetRotation(p.pane5Rotation);
      jp["pane6_rotation"]     = savePresetRotation(p.pane6Rotation);
      jp["rotation_interval_s"] = p.rotationIntervalS;
      jp["prop_overlay"]       = propOverlayToStr(p.propOverlay);
      {
        auto arr = nlohmann::json::array();
        for (auto t : p.propRotation)
          arr.push_back(propOverlayToStr(t));
        jp["prop_rotation"] = arr;
      }
      jp["weather_overlay"]    = weatherOverlayToStr(p.weatherOverlay);
      jp["map_style"]          = p.mapStyle;
      jp["map_night_lights"]   = p.mapNightLights;
      jp["show_grid"]          = p.showGrid;
      jp["grid_type"]          = p.gridType;
      jp["prop_band"]          = p.propBand;
      jp["prop_mode"]          = p.propMode;
      jp["prop_power"]         = p.propPower;
      jp["prop_toa"]           = p.propToa;
      jp["prop_path"]          = p.propPath;
      jp["prop_ant_gain"]      = p.propAntGain;
      presetsArr.push_back(jp);
    }
    json["presets"] = presetsArr;
  }

  // Deleted factory presets
  {
    auto deletedArr = nlohmann::json::array();
    for (const auto &name : config.deletedFactoryPresets) {
      deletedArr.push_back(name);
    }
    json["deleted_factory_presets"] = deletedArr;
  }

  std::filesystem::path tmpPath = configPath_;
  tmpPath.replace_extension(".json.tmp");

  std::ofstream ofs(tmpPath);
  if (!ofs) {
    std::fprintf(stderr, "ConfigManager: cannot write %s\n",
                 tmpPath.string().c_str());
    return false;
  }

  ofs << json.dump(2) << "\n";
  ofs.close();

  if (!ofs.good()) {
    std::fprintf(stderr, "ConfigManager: write failed for %s\n",
                 tmpPath.string().c_str());
    std::error_code ec;
    std::filesystem::remove(tmpPath, ec);
    return false;
  }

  // Atomic rename
  std::filesystem::rename(tmpPath, configPath_, ec);
  if (ec) {
    std::fprintf(stderr, "ConfigManager: rename failed from %s to %s: %s\n",
                 tmpPath.string().c_str(), configPath_.string().c_str(),
                 ec.message().c_str());
    return false;
  }

#ifdef __EMSCRIPTEN__
  // Flush MEMFS -> IndexedDB.  Retry once after 1 s on failure (handles
  // the case where the browser closes the tab before the first attempt
  // completes — the retry gives a second chance to persist the write).
  EM_ASM({
    function doSync(retriesLeft) {
      FS.syncfs(
          false, function(err) {
            if (err) {
              console.error('[IDBFS] Flush to IndexedDB failed (retries=' +
                                retriesLeft + '):',
                            err);
              if (retriesLeft > 0)
                setTimeout(function() { doSync(retriesLeft - 1); }, 1000);
            } else {
              console.log('[IDBFS] Config flushed to IndexedDB');
            }
          });
    }
    doSync(1);
  });
#endif

  return ofs.good();
}

void ConfigManager::applyPreset(AppConfig &config, int index) {
  if (index < 0 || index >= (int)config.presets.size())
    return;

  const auto &p = config.presets[index];
  config.pane1Rotation = p.pane1Rotation;
  config.pane2Rotation = p.pane2Rotation;
  config.pane3Rotation = p.pane3Rotation;
  config.pane4Rotation = p.pane4Rotation;
  config.pane5Rotation = p.pane5Rotation;
  config.pane6Rotation = p.pane6Rotation;
  config.rotationIntervalS = p.rotationIntervalS;
  config.propOverlay = p.propOverlay;
  config.propRotation = p.propRotation;
  config.weatherOverlay = p.weatherOverlay;
  config.mapStyle = p.mapStyle;
  config.mapNightLights = p.mapNightLights;
  config.showGrid = p.showGrid;
  config.gridType = p.gridType;
  config.propBand = p.propBand;
  config.propMode = p.propMode;
  config.propPower = p.propPower;
  config.propToa = p.propToa;
  config.propPath = p.propPath;
  config.propAntGain = p.propAntGain;
}

void ConfigManager::savePreset(AppConfig &config, const std::string &name) {
  ConfigPreset p;
  p.name = name;
  p.pane1Rotation = config.pane1Rotation;
  p.pane2Rotation = config.pane2Rotation;
  p.pane3Rotation = config.pane3Rotation;
  p.pane4Rotation = config.pane4Rotation;
  p.pane5Rotation = config.pane5Rotation;
  p.pane6Rotation = config.pane6Rotation;
  p.rotationIntervalS = config.rotationIntervalS;
  p.propOverlay = config.propOverlay;
  p.propRotation = config.propRotation;
  p.weatherOverlay = config.weatherOverlay;
  p.mapStyle = config.mapStyle;
  p.mapNightLights = config.mapNightLights;
  p.showGrid = config.showGrid;
  p.gridType = config.gridType;
  p.propBand = config.propBand;
  p.propMode = config.propMode;
  p.propPower = config.propPower;
  p.propToa = config.propToa;
  p.propPath = config.propPath;
  p.propAntGain = config.propAntGain;
  config.presets.push_back(std::move(p));
}

void ConfigManager::deletePreset(AppConfig &config, int index) {
  if (index >= 0 && index < (int)config.presets.size()) {
    const auto &presetName = config.presets[index].name;
    // Track deletion of factory presets so they don't reappear on update
    static const std::set<std::string> factoryPresetNames = {
        "DX", "Contest", "Satellite", "Environment/WX", "Prop Firehose", "DE Station Status"};
    if (factoryPresetNames.find(presetName) != factoryPresetNames.end()) {
      config.deletedFactoryPresets.insert(presetName);
    }
    config.presets.erase(config.presets.begin() + index);
  }
}
