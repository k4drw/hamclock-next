#pragma once

#include <ctime>
#include <string>

struct SatelliteTLE {
  std::string name;  // Satellite name (trimmed)
  std::string line1; // TLE line 1
  std::string line2; // TLE line 2
  int noradId = 0;   // NORAD catalog number (from line 1)
};

// Result of observing a satellite from an observer position.
struct SatObservation {
  double azimuth = 0.0;   // degrees [0, 360)
  double elevation = 0.0; // degrees [-90, 90]
  double range = 0.0;     // km
  double rangeRate = 0.0; // km/s (positive = receding)
  bool visible = false;
};

// Sub-satellite point on the ground.
struct SubSatPoint {
  double lat = 0.0;       // degrees, north positive
  double lon = 0.0;       // degrees, east positive
  double altitude = 0.0;  // km above earth
  double footprint = 0.0; // footprint diameter in km
};

// A predicted pass (AOS to LOS).
struct SatPass {
  std::time_t aosTime = 0; // acquisition of signal (UTC)
  double aosAz = 0.0;      // azimuth at AOS (degrees)
  std::time_t losTime = 0; // loss of signal (UTC)
  double losAz = 0.0;      // azimuth at LOS (degrees)
  double maxEl = 0.0;      // max elevation during pass (degrees)
};

// Ground track point for orbit path rendering.
struct GroundTrackPoint {
  double lat = 0.0;
  double lon = 0.0;

  GroundTrackPoint() = default;
  GroundTrackPoint(double la, double lo) : lat(la), lon(lo) {}
};
