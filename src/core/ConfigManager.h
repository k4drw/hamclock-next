#pragma once

#include "WidgetType.h"

#include <filesystem>
#include <string>

#include <SDL2/SDL.h>

struct AppConfig {
  // Identity
  std::string callsign;
  std::string grid;
  double lat = 0.0;
  double lon = 0.0;

  // Appearance
  SDL_Color callsignColor = {255, 165, 0, 255}; // default orange
  std::string theme = "default";

  // Pane widget selection (top bar panes 1–3)
  WidgetType pane1Widget = WidgetType::SOLAR;
  WidgetType pane2Widget = WidgetType::DX_CLUSTER;
  WidgetType pane3Widget = WidgetType::LIVE_SPOTS;

  // Panel state
  std::string panelMode = "dx";  // "dx" or "sat"
  std::string selectedSatellite; // satellite name (empty = none)
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
