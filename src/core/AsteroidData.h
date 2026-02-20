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
