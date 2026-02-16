#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct ActiveBeacon {
  int index;     // Beacon index (0-17)
  int bandIndex; // Band index (0-4)
};

struct BeaconScheduleInfo {
  std::string callsign;
  std::string location;
  double lat = 0.0;
  double lon = 0.0;
  int frequencyKhz = 0;
  int beaconIndex = 0;
  int bandIndex = 0;
  bool isActive = false;
  int secondsUntilTransmit = 0;
};

// NCDXF/IARU International Beacon Project
// 18 beacons transmitting on 5 bands in a coordinated 3-minute cycle
// Each beacon transmits for 10 seconds on each band in sequence
class BeaconProvider {
public:
  BeaconProvider();

  // Get currently active beacons (one per band)
  std::vector<ActiveBeacon> getActiveBeacons() const;

  // Get progress within current 10-second slot (0.0 to 1.0)
  float getSlotProgress() const;

  // Get current slot number (0-17)
  int getCurrentSlot() const;

  // Get seconds remaining until next slot
  int getSecondsUntilNextSlot() const;

  // Get detailed schedule info for a specific beacon/band combination
  BeaconScheduleInfo getScheduleInfo(int beaconIndex, int bandIndex) const;

  // Get upcoming beacon transmissions
  std::vector<BeaconScheduleInfo> getUpcomingBeacons(int count = 6) const;

  // Debug information for MCP/API
  nlohmann::json getDebugInfo() const;
};
