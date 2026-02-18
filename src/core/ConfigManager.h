#pragma once

#include "WidgetType.h"

#include <filesystem>
#include <string>
#include <vector>

#include <SDL.h>

enum class LiveSpotSource { PSK, RBN, WSPR };
enum class PropOverlayType { None, Muf, Voacap };

struct AppConfig {
  // Identity
  std::string callsign;
  std::string grid;
  double lat = 0.0;
  double lon = 0.0;

  // Appearance
  SDL_Color callsignColor = {255, 165, 0, 255}; // default orange
  std::string theme = "default";
  bool mapNightLights = true;
  bool useMetric = true;
  std::string projection = "equirectangular"; // or "robinson"
  std::string mapStyle = "nasa";              // "nasa", "terrain", "countries"
  bool showGrid = false;
  std::string gridType = "latlon"; // "latlon" or "maidenhead"
  PropOverlayType propOverlay = PropOverlayType::None;
  std::string propBand = "20m";
  std::string propMode = "SSB";
  int propPower = 100;   // Watts
  int mufRtOpacity = 40; // percentage
  bool showSatTrack = true; // Show satellite ground track line on world map

  // Pane widget selection (top bar panes 1–3)
  // Pane widget selection (rotation sets)
  std::vector<WidgetType> pane1Rotation = {WidgetType::SOLAR};
  std::vector<WidgetType> pane2Rotation = {WidgetType::DX_CLUSTER};
  std::vector<WidgetType> pane3Rotation = {WidgetType::LIVE_SPOTS};
  std::vector<WidgetType> pane4Rotation = {WidgetType::BAND_CONDITIONS};
  int rotationIntervalS = 30;

  // Panel state
  std::string panelMode = "dx";  // "dx" or "sat"
  std::string selectedSatellite; // satellite name (empty = none)

  // DX Cluster
  bool dxClusterEnabled = true;
  std::string dxClusterHost = "dxusa.net";
  int dxClusterPort = 7300;
  std::string dxClusterLogin = "";
  bool dxClusterUseWSJTX = false; // If true, ignore host and use UDP port

  // Live Spots (Combined RBN, PSK Reporter, WSPR)
  LiveSpotSource liveSpotSource = LiveSpotSource::PSK;
  bool liveSpotsOfDe =
      true; // true if spots OF de (de is sender), false if BY de
  bool liveSpotsUseCall = true; // true if filter by callsign, false if by grid
  int liveSpotsMaxAge = 30;     // minutes
  uint32_t liveSpotsBands = 0xFFF; // Bitmask of selected bands (lower 12 bits)
  bool rbnEnabled =
      false; // Kept for backward compat in logic, but mostly internal now
  std::string rbnHost = "telnet.reversebeacon.net";
  int rbnPort = 7000;

  // SDO Widget settings
  std::string sdoWavelength = "0193";
  bool sdoGrayline = false;
  bool sdoShowMovie = false;

  // Power / Screen
  bool preventSleep = true; // true to call SDL_DisableScreenSaver()

  // Rotator (Hamlib rotctld)
  std::string rotatorHost = "";  // Empty = disabled
  int rotatorPort = 4533;        // Default Hamlib rotctld port
  bool rotatorAutoTrack = false; // Auto-track satellite when enabled

  // Rig (Hamlib rigctld)
  std::string rigHost = ""; // Empty = disabled
  int rigPort = 4532;       // Default Hamlib rigctld port
  bool rigAutoTune = true;  // Auto-tune when clicking DX spots

  // QRZ
  std::string qrzUsername;
  std::string qrzPassword;

  // Countdown
  std::string countdownLabel;
  std::string countdownTime;

  // Brightness
  int brightness = 100;
  bool brightnessSchedule = false;
  int dimHour = 22;
  int dimMinute = 0;
  int brightHour = 6;
  int brightMinute = 0;

  // RSS
  bool rssEnabled = true; // Show RSS news banner

  // Activity panels
  std::string ontaFilter = "all"; // "all", "pota", or "sota"

  // Security
  bool gpsEnabled = false;

  // Network (WASM)
  // CORS proxy prefix prepended to all external URLs in the WASM build.
  // Default "/proxy/" works with the bundled serve.py and the nginx snippet.
  // Set to "" to disable (only if your server sends CORS headers itself).
#ifdef __EMSCRIPTEN__
  std::string corsProxyUrl = "/proxy/";
#else
  std::string corsProxyUrl = "";
#endif
};

class ConfigManager {
public:
  // Resolves the config directory and file path.
  // Returns false if the path could not be determined.
  bool init();

  // Load config from disk. Returns false if file is missing or invalid.
  bool load(AppConfig &config) const;

  // Save config to disk. Creates directories if needed. Returns false on
  // failure.
  bool save(const AppConfig &config) const;

  // Returns the resolved config file path (valid after init()).
  const std::filesystem::path &configPath() const { return configPath_; }
  const std::filesystem::path &configDir() const { return configDir_; }

private:
  std::filesystem::path configDir_;
  std::filesystem::path configPath_;
};
