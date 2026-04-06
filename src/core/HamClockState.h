#pragma once

#include "Astronomy.h"

#include <chrono>
#include <map>
#include <mutex>
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
  double dxFreqKhz = 0.0;

  // Panel-driven spot selection (DX Cluster / ONTA)
  // Non-empty when a panel spot is selected; overrides map-click display.
  std::string dxCallsign;

  // Saved map-click state — restored when panel spot is deselected
  LatLon mapDxLocation = {0, 0};
  std::string mapDxGrid;
  bool mapDxActive = false;

  // DX timezone — populated async by ZoneDetect lookup, guarded by locationMutex
  std::string dxTzId;                     // IANA ID, e.g. "America/New_York"
  int    dxTzOffset = 0;                  // UTC offset in whole hours (DST-aware on Linux/macOS)
  bool   dxTzValid  = false;             // true once lookup result is stored
  LatLon dxTzQueryLoc = {-999.0, 0.0};  // sentinel: triggers fetch on first DX selection

  // Telemetry
  float fps = 0.0f;
  std::map<std::string, ServiceStatus> services;

  // Asteroid selection — "" means none selected
  std::string selectedAsteroidName;

  // Synchronization
  // locationMutex: protects deLocation/deGrid/deCallsign/dxLocation/dxGrid/
  //   dxActive/dxCallsign/mapDx*/fps.  Held by the main thread for the
  //   duration of each update()+render() call; acquired briefly by the
  //   WebServer thread for reads/writes and by worker threads that read
  //   location fields.
  mutable std::mutex locationMutex;
  // servicesMutex: protects the services map.  Acquired by each service
  //   provider thread on every status write, and by the render/web threads
  //   when iterating the map.
  mutable std::mutex servicesMutex;
};
