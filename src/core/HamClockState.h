#pragma once

#include "Astronomy.h"

#include <chrono>
#include <map>
#include <string>

struct ServiceStatus {
  bool ok = false;
  std::string lastError;
  std::chrono::system_clock::time_point lastSuccess{};
};

struct HamClockState {
  // DE (home) station — set from config
  LatLon deLocation = {0, 0};
  std::string deCallsign;
  std::string deGrid;

  // DX (target) station — set by map clicks
  LatLon dxLocation = {0, 0};
  std::string dxGrid;
  bool dxActive = false;

  // Panel-driven spot selection (DX Cluster / ONTA)
  // Non-empty when a panel spot is selected; overrides map-click display.
  std::string dxCallsign;

  // Saved map-click state — restored when panel spot is deselected
  LatLon mapDxLocation = {0, 0};
  std::string mapDxGrid;
  bool mapDxActive = false;

  // Telemetry
  float fps = 0.0f;
  std::map<std::string, ServiceStatus> services;

  // Asteroid selection — "" means none selected
  std::string selectedAsteroidName;
};
