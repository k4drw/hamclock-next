#include "BeaconProvider.h"
#include "../core/BeaconData.h"
#include "../core/Logger.h"

BeaconProvider::BeaconProvider() {
  LOG_I("BeaconProvider", "Initialized with {} NCDXF beacons", NCDXF_BEACONS.size());
}

std::vector<ActiveBeacon> BeaconProvider::getActiveBeacons() const {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  struct tm *gmt = std::gmtime(&now_c);

  int totalSecs = gmt->tm_hour * 3600 + gmt->tm_min * 60 + gmt->tm_sec;
  int slot = (totalSecs % 180) / 10;

  std::vector<ActiveBeacon> active;
  for (int band = 0; band < 5; ++band) {
    // NCDXF algorithm: beacon b is on band f if (b + f) % 18 == slot
    // => b = (slot - f) % 18
    int b = (slot - band + 18) % 18;
    active.push_back({b, band});
  }
  return active;
}

float BeaconProvider::getSlotProgress() const {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  return (float)(now_c % 10) / 10.0f;
}

int BeaconProvider::getCurrentSlot() const {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  struct tm *gmt = std::gmtime(&now_c);

  int totalSecs = gmt->tm_hour * 3600 + gmt->tm_min * 60 + gmt->tm_sec;
  return (totalSecs % 180) / 10;
}

int BeaconProvider::getSecondsUntilNextSlot() const {
  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  return 10 - (now_c % 10);
}

BeaconScheduleInfo BeaconProvider::getScheduleInfo(int beaconIndex, int bandIndex) const {
  if (beaconIndex < 0 || beaconIndex >= (int)NCDXF_BEACONS.size()) {
    return BeaconScheduleInfo{};
  }
  if (bandIndex < 0 || bandIndex >= (int)BEACON_BANDS.size()) {
    return BeaconScheduleInfo{};
  }

  const Beacon &beacon = NCDXF_BEACONS[beaconIndex];

  BeaconScheduleInfo info;
  info.callsign = beacon.callsign;
  info.location = beacon.location;
  info.lat = beacon.lat;
  info.lon = beacon.lon;
  info.frequencyKhz = BEACON_BANDS[bandIndex];
  info.beaconIndex = beaconIndex;
  info.bandIndex = bandIndex;

  // Calculate when this beacon will transmit on this band
  // Beacon b on band f transmits at slot (b + f) % 18
  int slot = (beaconIndex + bandIndex) % 18;

  int currentSlot = getCurrentSlot();
  int slotsUntil = (slot - currentSlot + 18) % 18;
  if (slotsUntil == 0) {
    info.isActive = true;
    info.secondsUntilTransmit = 0;
  } else {
    info.isActive = false;
    info.secondsUntilTransmit = slotsUntil * 10;
  }

  return info;
}

std::vector<BeaconScheduleInfo> BeaconProvider::getUpcomingBeacons(int count) const {
  std::vector<BeaconScheduleInfo> upcoming;

  int currentSlot = getCurrentSlot();

  for (int i = 0; i < count && i < 18; ++i) {
    int slot = (currentSlot + i) % 18;

    // For each slot, get all 5 beacons (one per band)
    for (int band = 0; band < 5; ++band) {
      int beaconIdx = (slot - band + 18) % 18;
      auto info = getScheduleInfo(beaconIdx, band);
      info.secondsUntilTransmit = i * 10;
      upcoming.push_back(info);
    }
  }

  return upcoming;
}

nlohmann::json BeaconProvider::getDebugInfo() const {
  nlohmann::json j;

  j["current_slot"] = getCurrentSlot();
  j["slot_progress"] = getSlotProgress();
  j["seconds_until_next"] = getSecondsUntilNextSlot();

  auto active = getActiveBeacons();
  nlohmann::json activeArray = nlohmann::json::array();
  for (const auto &a : active) {
    nlohmann::json beacon;
    beacon["index"] = a.index;
    beacon["callsign"] = NCDXF_BEACONS[a.index].callsign;
    beacon["band_index"] = a.bandIndex;
    beacon["frequency_khz"] = BEACON_BANDS[a.bandIndex];
    activeArray.push_back(beacon);
  }
  j["active_beacons"] = activeArray;

  return j;
}
