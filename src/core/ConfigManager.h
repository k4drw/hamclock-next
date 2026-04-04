#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <SDL.h>

enum class LiveSpotSource { PSK, RBN, WSPR };
enum class PropOverlayType {
  None,
  Muf,
  Voacap,
  Reliability,
  Toa,
  Heatmap,
  Drap,
  Aurora
};
enum class WeatherOverlayType { None, WxMb, CloudsGrib };
enum class HubMode { Off, Master, Client };

struct ReminderEntry {
  std::string label;
  std::string date; // YYYY-MM-DD
  bool active = true;
  std::string acknowledgedDate; // If == date, notification has been dismissed
};

struct ConfigPreset {
  std::string name;
  std::vector<std::string> pane1Rotation;
  std::vector<std::string> pane2Rotation;
  std::vector<std::string> pane3Rotation;
  std::vector<std::string> pane4Rotation;
  std::vector<std::string> pane5Rotation;
  std::vector<std::string> pane6Rotation;
  int rotationIntervalS = 30;
  PropOverlayType propOverlay = PropOverlayType::None;
  WeatherOverlayType weatherOverlay = WeatherOverlayType::None;
  std::string mapStyle = "nasa";
  bool mapNightLights = true;
  bool showGrid = false;
  std::string gridType = "latlon";
  std::string propBand = "20m";
  std::string propMode = "SSB";
  int propPower = 100;
  float propToa = 3.0f;
  int propPath = 0; // 0=Short, 1=Long
};

struct AppConfig {
  // Identity
  std::string callsign;
  std::string grid;
  double lat = 0.0;
  double lon = 0.0;
  // Appearance
  SDL_Color callsignColor = {255, 165, 0, 255}; // default orange
  SDL_Color callsignBgColor = {0, 0, 0, 0};     // default transparent (no bg)
  std::string theme = "default";
  bool mapNightLights = true;
  bool useMetric = true;
  std::string projection = "equirectangular"; // or "robinson"
  std::string mapStyle = "nasa";              // "nasa", "topo", "topo_bathy"
  bool showGrid = false;
  std::string gridType = "latlon"; // "latlon" or "maidenhead"
  PropOverlayType propOverlay = PropOverlayType::None;
  WeatherOverlayType weatherOverlay = WeatherOverlayType::None;
  std::string propBand = "20m";
  std::string propMode = "SSB";
  int propPower = 100;      // Watts
  float propToa = 3.0f;     // Take-off angle (degrees)
  int propPath = 0;         // 0=Short, 1=Long
  int mufRtOpacity = 40;    // percentage
  bool showSatTrack = true; // Show satellite ground track line on world map
  bool showBeacons = true;  // Show NCDXF beacons on world map
  bool showBorders = false; // Show Natural Earth country borders
  std::string displayPowerMethod =
      "auto"; // "auto", "vcgencmd", "bl_power", etc.
  std::string logLevel = "warn"; // "trace","debug","info","warn","error"

  // Pane widget selection (rotation sets, stored as typeId strings)
  std::vector<std::string> pane1Rotation = {"solar"};
  std::vector<std::string> pane2Rotation = {"dx_cluster"};
  std::vector<std::string> pane3Rotation = {"live_spots"};
  std::vector<std::string> pane4Rotation = {"band_conditions"};
  std::vector<std::string> pane5Rotation = {"de_info"};
  std::vector<std::string> pane6Rotation = {"dx_info"};
  int rotationIntervalS = 30;
  bool syncRotation =
      false; // true = all panes advance on the same wall-clock tick
  std::vector<std::string> watchlist; // persisted callsign watchlist

  // Panel state
  std::string panelMode = "dx";        // "dx" or "sat" (for DX_INFO widget)
  std::string selectedSatellite;       // satellite name for DX_INFO widget
  std::string satWidgetSatellite;      // satellite name for standalone SATELLITE widget
  std::vector<int> customSatelliteSCCs;

  // DX Cluster
  bool dxClusterEnabled = true;
  std::string dxClusterHost = "dxusa.net";
  int dxClusterPort = 7300;
  std::string dxClusterLogin = "";
  bool dxClusterUseWSJTX = false; // If true, ignore host and use UDP port
  int wsjtxPort = 2237;           // UDP port for WSJT-X listener

  // Live Spots (Combined RBN, PSK Reporter, WSPR)
  LiveSpotSource liveSpotSource = LiveSpotSource::PSK;
  bool liveSpotsOfDe =
      true; // true if spots OF de (de is sender), false if BY de
  bool liveSpotsUseCall = true; // true if filter by callsign, false if by grid
  int liveSpotsMaxAge = 30;     // minutes
  uint32_t liveSpotsBands = 0xFFF; // Bitmask of selected bands (lower 12 bits)
  std::string pskrProxyUrl;        // Custom PSK Reporter proxy URL
  bool rbnEnabled =
      false; // Kept for backward compat in logic, but mostly internal now
  std::string rbnHost = "telnet.reversebeacon.net";
  int rbnPort = 7000;

  // SDO Widget settings
  std::string sdoWavelength = "0193";
  bool sdoRotating = false;
  bool sdoPfss = false;
  bool sdoShowMovie = false;

  // Marine Widget settings
  std::string marineStation;
  std::string marineBuoy;

  // Power / Screen
  bool preventSleep = true; // true to call SDL_DisableScreenSaver()

  // Rotator (Hamlib rotctld)
  std::string rotatorHost = "";  // Empty = disabled
  int rotatorPort = 4533;        // Default Hamlib rotctld port
  bool rotatorAutoTrack = false; // Auto-track satellite when enabled
  bool rotatorUpover = false;    // Azimuth flip for zenith passes

  // Rig (Hamlib rigctld)
  std::string rigHost = ""; // Empty = disabled
  int rigPort = 4532;       // Default Hamlib rigctld port
  bool rigAutoTune = true;  // Auto-tune when clicking DX spots

  // QRZ
  std::string qrzUsername;
  std::string qrzPassword;

  // Additional Services
  std::string repeaterBookKey;
  std::string winlinkKey;

  // Daily alarm
  bool alarmArmed = false;
  int alarmTimeHH = 7;
  int alarmTimeMM = 0;
  bool alarmUtc = true;

  // One-time alarm (ISO8601 date-time string, UTC)
  bool onceAlarmArmed = false;
  std::string onceAlarmTime; // e.g. "2026-03-15T14:00"

  // Countdown
  std::string countdownLabel;
  std::string countdownTime;

  // Reminder
  std::string callsignExpiry;
  std::string callsignFrn;
  std::string
      callsignExpiryAcknowledged;      // If == callsignExpiry, auto row acked
  int64_t callsignExpiryLastCheck = 0; // epoch seconds; 0 = never checked
  std::vector<ReminderEntry> reminders;

  // Brightness / display schedule
  int brightness = 100;
  bool brightnessSchedule = false;
  int dimHour = 22;
  int dimMinute = 0;
  int brightHour = 6;
  int brightMinute = 0;
  int idleMinutes = 0; // 0 = no idle blanking
  bool ltr329AutoDim = false;

  // Map view
  double mapCenterLon = 0.0;
  bool centerMapOnDe = false;
  int mapPanX = 0;
  int mapPanY = 0;
  double mapZoom = 1.0;

  // RSS
  bool rssEnabled = true; // Show RSS news banner

  // Activity panels
  std::string ontaFilter = "all"; // "all", "pota", or "sota"
  int ontaMaxDistKm = 0;          // 0 = no distance filter

  // Asteroid widget
  std::string asteroidIcon = "☄";
  SDL_Color asteroidColor = {255, 140, 0, 255}; // default orange

  // Security
  bool gpsEnabled = false;

  // Font
  std::string fontPath = ""; // Empty = use embedded font

  // Audio
  bool audioMuted = false;

  // Aux Clock timezone
  int auxClockTzOffset = 0;            // Hours from UTC (-12 to +14)
  std::string auxClockTzLabel = "UTC"; // Display label
  int auxClockStarMode = 1;

  // Big Clock
  bool bigClockDigital  = true;
  bool bigClock12h      = false;
  bool bigClockUtc      = false;
  bool bigClockShowSec  = true;
  bool bigClockShowDate = true;
  uint8_t bigClockHue   = 85; // default green (matches original NV_SWHUE default)

  // Update
  std::string skippedVersion;

  // Local Data Hub
  HubMode hubMode = HubMode::Off;
  std::string hubIp = "";
  int hubPort = 8080;

  // Network (WASM)
  // CORS proxy prefix prepended to all external URLs in the WASM build.
  // Default "/proxy/" works with the bundled serve.py and the nginx snippet.
  // Set to "" to disable (only if your server sends CORS headers itself).
#ifdef __EMSCRIPTEN__
  std::string corsProxyUrl = "/proxy/";
#else
  std::string corsProxyUrl = "";
#endif
  std::map<std::string, SDL_Color> colorOverrides;

  // Contest Mode
  bool contestModeActive = false; // true while contest preset is active

  // Calendar notifications
  int calendarNotifyMinutes = 10;       // minutes before event to show alert
  int calendarAllDayNotifyHour = 8;     // hour (UTC) to alert for all-day events
  int calendarDismissMinutes = 10;      // minutes before auto-dismissing the alert overlay

  // Presets
  std::vector<ConfigPreset> presets;
};

class ConfigManager {
public:
  static ConfigManager &instance() {
    static ConfigManager inst;
    return inst;
  }

  // Delete copy and move
  ConfigManager(const ConfigManager &) = delete;
  ConfigManager &operator=(const ConfigManager &) = delete;
  ConfigManager(ConfigManager &&) = delete;
  ConfigManager &operator=(ConfigManager &&) = delete;

  AppConfig &getConfig() { return config_; }
  const AppConfig &getConfig() const { return config_; }

  // Resolves the config directory and file path.
  // Returns false if the path could not be determined.
  bool init();

  // Load config from disk. Returns false if file is missing or invalid.
  bool load(AppConfig &config);

  // Save config to disk. Creates directories if needed. Returns false on
  // failure.
  bool save(const AppConfig &config);

  // Apply a preset to the given config
  void applyPreset(AppConfig &config, int index);

  // Save current config as a new preset
  void savePreset(AppConfig &config, const std::string &name);

  // Delete a preset
  void deletePreset(AppConfig &config, int index);

  // Returns the resolved config file path (valid after init()).
  const std::filesystem::path &configPath() const { return configPath_; }
  const std::filesystem::path &configDir() const { return configDir_; }

private:
  ConfigManager() = default;
  AppConfig config_;
  std::filesystem::path configDir_;
  std::filesystem::path configPath_;
};
