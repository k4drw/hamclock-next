#pragma once

#include <string>
#include <vector>
#include <chrono>

namespace HamClock {

struct MeteorShower {
  std::string name;
  int startMonth;
  int startDay;
  int endMonth;
  int endDay;
  int peakMonth;
  int peakDay;
  int zhr; // Zenithal Hourly Rate
};

struct MeteorData {
  bool valid = false;
  float currentIndex; // 0.0 to 10.0
  std::vector<std::string> activeShowers;
  std::chrono::system_clock::time_point nextDiurnalPeak;
  float hourlyActivity[24]; // 24-hour prediction
};

} // namespace HamClock
