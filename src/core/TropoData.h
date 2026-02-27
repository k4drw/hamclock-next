#pragma once

#include <string>
#include <vector>

namespace HamClock {

enum class TropoLevel {
  Insignificant,
  VeryPoor,
  Poor,
  Fair,
  Good,
  VeryGood,
  Strong,
  VeryStrong,
  Extreme
};

struct TropoForecast {
  int hourOffset; // Hours from now
  TropoLevel level;
  std::string description;
};

struct TropoData {
  bool valid = false;
  std::string location;
  TropoLevel currentLevel;
  std::vector<TropoForecast> forecast;
};

inline const char* tropoLevelToString(TropoLevel l) {
  switch (l) {
    case TropoLevel::Insignificant: return "Insignificant";
    case TropoLevel::VeryPoor: return "Very Poor";
    case TropoLevel::Poor: return "Poor";
    case TropoLevel::Fair: return "Fair";
    case TropoLevel::Good: return "Good";
    case TropoLevel::VeryGood: return "Very Good";
    case TropoLevel::Strong: return "Strong";
    case TropoLevel::VeryStrong: return "Very Strong";
    case TropoLevel::Extreme: return "Extreme";
  }
  return "Unknown";
}

} // namespace HamClock
