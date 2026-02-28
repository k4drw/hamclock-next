#pragma once

#include "AsteroidData.h"
#include "../core/SatelliteTypes.h"
#include <vector>

class AsteroidPropagator {
public:
  // Compute sub-asteroid ground track.
  // jd_start/end: Julian date range. steps: number of sample points.
  // Returns empty vector if elements invalid.
  static std::vector<GroundTrackPoint>
  computeGroundTrack(const OrbitalElements &elem,
                     double jd_start, double jd_end, int steps = 48);

  // Single-point sub-asteroid lat/lon at a given Julian Date.
  // Returns false if computation fails.
  static bool subAsteroidPoint(const OrbitalElements &elem,
                               double jd, double &lat, double &lon);

  // Compute azimuth (deg, N-clockwise) and elevation (deg) of an asteroid
  // as seen from observer (obsLat/obsLon, degrees) given its sub-point
  // (subLat/subLon, degrees).  el < 0 means below horizon.
  static void computeAzEl(double obsLat, double obsLon,
                           double subLat, double subLon,
                           double &az, double &el);

private:
  static double solveKepler(double M, double e, int maxIter = 20);
  static void earthEclipticXYZ(double jd, double &x, double &y, double &z);
};
