#pragma once

#include "Astronomy.h"
#include <chrono>
#include <string>
#include <vector>

namespace HamClock {

struct GreylineWindow {
  std::chrono::system_clock::time_point startTime;
  std::chrono::system_clock::time_point endTime;
  double durationMinutes;
  std::string deType; // "Sunrise" or "Sunset"
  std::string dxType; // "Sunrise" or "Sunset"
  bool valid = false;
};

class GreylineCalculator {
public:
  // Find the next mutual greyline window for DE and DX locations.
  // Uses a coarse-to-fine scan for performance on low-power devices.
  static GreylineWindow findNextOverlap(LatLon de, LatLon dx,
                                        std::chrono::system_clock::time_point now);

private:
  // Returns true if the sun elevation at the given location is in the greyline zone.
  // Zone: -12.0 (Nautical Twilight) to +2.0 (Refraction/alt allowance)
  static bool isInGreyline(LatLon loc, std::chrono::system_clock::time_point tp, std::string& type);
};

} // namespace HamClock
