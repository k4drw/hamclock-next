#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

enum class SpaceWxAlertType {
  KIndex,      // K-index threshold
  SolarFlareM, // M-class solar flare
  SolarFlareX, // X-class solar flare
  CMEArrival,  // CME arrival / predicted impact
  ProtonEvent, // S-class proton event
  GeoStorm,    // Geomagnetic storm (G-scale)
  Other        // Uncategorized
};

struct SpaceWxAlert {
  std::string issueTime; // "YYYY-MM-DD HH:MM"
  SpaceWxAlertType type;
  std::string typeLabel; // e.g. "ALERT: Geomagnetic K-index of 6"
  std::string summary;   // secondary detail line (may be empty)
};

struct SpaceWxAlertData {
  std::vector<SpaceWxAlert> alerts; // most recent first, max 20
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class SpaceWeatherAlertStore {
public:
  void update(const SpaceWxAlertData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  SpaceWxAlertData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  SpaceWxAlertData data_;
};
