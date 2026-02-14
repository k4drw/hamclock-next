#pragma once

#include "WidgetType.h"

#include <filesystem>
#include <string>
#include <vector>

#include <SDL.h>

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

  // SDO Widget settings
  std::string sdoWavelength = "0193";
  bool sdoGrayline = false;
  bool sdoShowMovie = false;

  // PSK Reporter
  bool pskOfDe = true;    // true if spots OF de (de is sender), false if BY de
  bool pskUseCall = true; // true if filter by callsign, false if by grid
  int pskMaxAge = 30;     // minutes
  uint32_t pskBands = 0xFFF; // Bitmask of selected bands (lower 12 bits)

  // Power / Screen
  bool preventSleep = true; // true to call SDL_DisableScreenSaver()
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
