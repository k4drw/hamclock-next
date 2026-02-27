#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace HamClock {

struct LightningStrike {
  double lat;
  double lon;
  double distanceKm;
  double bearing;
  std::chrono::system_clock::time_point time;
};

struct LightningData {
  bool valid = false;
  int strikesLast10Min = 0;
  int strikesLast30Min = 0;
  double closestStrikeKm = -1.0;
  double closestBearing = 0.0;
  bool safetyAlert = false; // Strike within 15km (9mi)
  
  std::vector<LightningStrike> recentStrikes;
};

} // namespace HamClock
