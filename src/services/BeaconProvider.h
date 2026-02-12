#pragma once

#include <chrono>
#include <vector>

struct ActiveBeacon {
  int index;
  int bandIndex;
};

class BeaconProvider {
public:
  std::vector<ActiveBeacon> getActiveBeacons() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm *gmt = std::gmtime(&now_c);

    int totalSecs = gmt->tm_hour * 3600 + gmt->tm_min * 60 + gmt->tm_sec;
    int slot = (totalSecs % 180) / 10;

    std::vector<ActiveBeacon> active;
    for (int band = 0; band < 5; ++band) {
      // Beacon b is on band f if (b + f) % 18 == slot
      // => b = (slot - f) % 18
      int b = (slot - band + 18) % 18;
      active.push_back({b, band});
    }
    return active;
  }

  // Returns 0.0 to 1.0 for progress within current 10s slot
  float getSlotProgress() const {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    return (float)(now_c % 10) / 10.0f;
  }
};
