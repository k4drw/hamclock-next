#pragma once

#include <string>
#include <vector>

struct Asteroid {
  std::string name;
  std::string approachDate;      // YYYY-MM-DD
  std::string closeApproachTime; // HH:MM
  double missDistanceKm;
  double missDistanceLD;    // Lunar Distance
  double velocityKmS;       // Relative Velocity
  double julianDate;        // For reliable sorting
  double absoluteMagnitude; // H (brightness)
  bool isHazardous;         // Potentially Hazardous Asteroid
};

struct AsteroidData {
  std::vector<Asteroid> asteroids; // Sorted by date/distance
  std::string lastFetchTime;
  bool valid = false;
};

struct OrbitalElements {
  double e        = 0; // eccentricity
  double a        = 0; // semi-major axis (AU)
  double i        = 0; // inclination (deg)
  double om       = 0; // longitude of ascending node Ω (deg)
  double w        = 0; // argument of perihelion ω (deg)
  double ma       = 0; // mean anomaly at epoch M₀ (deg)
  double epoch_jd = 0; // epoch (Julian Date)
  bool valid      = false;
};
