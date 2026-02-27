#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace HamClock {

enum class SolarStormScale {
  None = 0,
  Minor = 1,
  Moderate = 2,
  Strong = 3,
  Severe = 4,
  Extreme = 5
};

struct SolarStormData {
  bool valid = false;
  
  // NOAA Scales (R, S, G)
  SolarStormScale rScale = SolarStormScale::None; // Radio Blackout (X-ray)
  SolarStormScale sScale = SolarStormScale::None; // Solar Radiation (Proton)
  SolarStormScale gScale = SolarStormScale::None; // Geomagnetic Storm (Kp)
  
  float xrayFlux = 0.0f; // Current Watts/m^2
  std::string lastFlareClass = "None"; // e.g., "X1.2"
  
  // Predictions & Timers
  bool cmeImpactPredicted = false;
  uint32_t cmeArrivalUtc = 0; // Unix timestamp
  std::string alertMessage;
  
  float fluxHistory[60]; // Last 60 minutes of X-ray flux
};

inline const char* scaleToString(SolarStormScale s) {
  switch(s) {
    case SolarStormScale::None: return "None";
    case SolarStormScale::Minor: return "Minor";
    case SolarStormScale::Moderate: return "Moderate";
    case SolarStormScale::Strong: return "Strong";
    case SolarStormScale::Severe: return "Severe";
    case SolarStormScale::Extreme: return "Extreme";
  }
  return "Unknown";
}

} // namespace HamClock
