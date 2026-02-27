#include "GreylineCalculator.h"
#include <algorithm>

namespace HamClock {

bool GreylineCalculator::isInGreyline(LatLon loc, std::chrono::system_clock::time_point tp, std::string& type) {
  SubSolarPoint sunPos = Astronomy::sunPosition(tp);
  double az, el;
  Astronomy::calculateAzEl(loc, sunPos, az, el);

  // Is it in the window?
  if (el >= -12.0 && el <= 2.0) {
    // Determine if it's sunrise or sunset by checking a minute later
    SubSolarPoint nextPos = Astronomy::sunPosition(tp + std::chrono::minutes(1));
    double nAz, nEl;
    Astronomy::calculateAzEl(loc, nextPos, nAz, nEl);
    type = (nEl > el) ? "Sunrise" : "Sunset";
    return true;
  }
  return false;
}

GreylineWindow GreylineCalculator::findNextOverlap(LatLon de, LatLon dx,
                                                   std::chrono::system_clock::time_point now) {
  GreylineWindow window;
  window.valid = false;

  // Coarse scan: 30 minute steps for the next 24 hours
  std::chrono::system_clock::time_point coarseStart = now;
  bool foundCoarse = false;
  std::chrono::system_clock::time_point matchTime;

  for (int i = 0; i < 48; ++i) {
    std::chrono::system_clock::time_point t = coarseStart + std::chrono::minutes(i * 30);
    std::string deT, dxT;
    if (isInGreyline(de, t, deT) && isInGreyline(dx, t, dxT)) {
      matchTime = t;
      foundCoarse = true;
      break;
    }
  }

  if (!foundCoarse) return window;

  // Fine refinement: Back up 30 minutes from the coarse match and scan in 1 minute steps
  std::chrono::system_clock::time_point scanStart = matchTime - std::chrono::minutes(35);
  bool inWindow = false;
  
  for (int i = 0; i < 70; ++i) {
    std::chrono::system_clock::time_point t = scanStart + std::chrono::minutes(i);
    if (t < now) continue;

    std::string deT, dxT;
    bool match = isInGreyline(de, t, deT) && isInGreyline(dx, t, dxT);

    if (match && !inWindow) {
      window.startTime = t;
      window.deType = deT;
      window.dxType = dxT;
      window.valid = true;
      inWindow = true;
    } else if (!match && inWindow) {
      window.endTime = t;
      inWindow = false;
      break;
    }
  }

  if (window.valid) {
    if (!inWindow && window.endTime > window.startTime) {
      auto diff = std::chrono::duration_cast<std::chrono::minutes>(window.endTime - window.startTime);
      window.durationMinutes = static_cast<double>(diff.count());
    } else {
      // If we are still in the window at the end of refinement, just cap it
      window.endTime = window.startTime + std::chrono::minutes(30);
      window.durationMinutes = 30.0;
    }
  }

  return window;
}

} // namespace HamClock
