#include "AsteroidPropagator.h"
#include <cmath>

static constexpr double DEG2RAD = M_PI / 180.0;
static constexpr double RAD2DEG = 180.0 / M_PI;
// Gaussian gravitational constant squared (AU³/day²)
static constexpr double k2 = 0.01720209895 * 0.01720209895;

// Solve Kepler's equation M = E - e*sin(E) by Newton-Raphson.
double AsteroidPropagator::solveKepler(double M, double e, int maxIter) {
  double E = M;
  for (int n = 0; n < maxIter; ++n) {
    double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
    E += dE;
    if (std::fabs(dE) < 1e-10)
      break;
  }
  return E;
}

// Low-precision Earth position in heliocentric ecliptic coordinates (AU).
void AsteroidPropagator::earthEclipticXYZ(double jd,
                                          double &x, double &y, double &z) {
  double T = (jd - 2451545.0) / 36525.0;
  // Mean longitude and anomaly (degrees)
  double L = std::fmod(280.460 + 36000.771 * T, 360.0);
  double g = std::fmod(357.528 + 35999.050 * T, 360.0) * DEG2RAD;
  // Ecliptic longitude (degrees → radians)
  double lambda =
      (L + 1.915 * std::sin(g) + 0.020 * std::sin(2.0 * g)) * DEG2RAD;
  // Heliocentric distance (AU)
  double r =
      1.00014 - 0.01671 * std::cos(g) - 0.00014 * std::cos(2.0 * g);
  x = r * std::cos(lambda);
  y = r * std::sin(lambda);
  z = 0.0;
}

bool AsteroidPropagator::subAsteroidPoint(const OrbitalElements &elem,
                                          double jd,
                                          double &lat, double &lon) {
  if (!elem.valid || elem.a <= 0)
    return false;

  // Mean motion (rad/day)
  double n = std::sqrt(k2 / (elem.a * elem.a * elem.a));

  // Propagate mean anomaly
  double M0 = elem.ma * DEG2RAD;
  double M = std::fmod(M0 + n * (jd - elem.epoch_jd), 2.0 * M_PI);
  if (M < 0)
    M += 2.0 * M_PI;

  // Eccentric anomaly
  double E = solveKepler(M, elem.e);

  // True anomaly
  double sinHalfE = std::sin(E / 2.0);
  double cosHalfE = std::cos(E / 2.0);
  double nu = 2.0 * std::atan2(
      std::sqrt(1.0 + elem.e) * sinHalfE,
      std::sqrt(1.0 - elem.e) * cosHalfE);

  // Heliocentric distance (AU)
  double r = elem.a * (1.0 - elem.e * std::cos(E));

  // Orbital-plane (perifocal) coordinates
  double xp = r * std::cos(nu);
  double yp = r * std::sin(nu);

  // Rotate perifocal → heliocentric ecliptic using ω, i, Ω
  double omega = elem.w  * DEG2RAD; // argument of perihelion
  double inc   = elem.i  * DEG2RAD; // inclination
  double Omega = elem.om * DEG2RAD; // longitude of ascending node

  double cosO = std::cos(Omega), sinO = std::sin(Omega);
  double cosI = std::cos(inc),   sinI = std::sin(inc);
  double coso = std::cos(omega), sino = std::sin(omega);

  double xe = xp * (cosO * coso - sinO * sino * cosI) +
              yp * (-cosO * sino - sinO * coso * cosI);
  double ye = xp * (sinO * coso + cosO * sino * cosI) +
              yp * (-sinO * sino + cosO * coso * cosI);
  double ze = xp * (sino * sinI) + yp * (coso * sinI);

  // Earth's heliocentric ecliptic position
  double xe_earth, ye_earth, ze_earth;
  earthEclipticXYZ(jd, xe_earth, ye_earth, ze_earth);

  // Geocentric ecliptic vector (asteroid − Earth)
  double gx = xe - xe_earth;
  double gy = ye - ye_earth;
  double gz = ze - ze_earth;

  // Rotate geocentric ecliptic → geocentric equatorial via obliquity ε
  static constexpr double eps = 23.4393 * DEG2RAD;
  double cosE = std::cos(eps), sinE = std::sin(eps);
  double qx = gx;
  double qy = gy * cosE - gz * sinE;
  double qz = gy * sinE + gz * cosE;

  double r_gc = std::sqrt(qx * qx + qy * qy + qz * qz);
  if (r_gc < 1e-10)
    return false;

  // Declination (latitude)
  lat = std::asin(qz / r_gc) * RAD2DEG;

  // Right ascension
  double ra = std::atan2(qy, qx);

  // Greenwich Mean Sidereal Time (Earth Rotation Angle) in radians
  double era = 2.0 * M_PI *
               (0.7790572732640 +
                1.00273781191135448 * (jd - 2451545.0));
  era = std::fmod(era, 2.0 * M_PI);

  // Sub-asteroid longitude
  double lon_rad = ra - era;
  // Normalize to [-π, π]
  lon_rad = std::fmod(lon_rad + M_PI, 2.0 * M_PI);
  if (lon_rad < 0)
    lon_rad += 2.0 * M_PI;
  lon_rad -= M_PI;

  lon = lon_rad * RAD2DEG;
  return true;
}

std::vector<GroundTrackPoint>
AsteroidPropagator::computeGroundTrack(const OrbitalElements &elem,
                                       double jd_start, double jd_end,
                                       int steps) {
  std::vector<GroundTrackPoint> track;
  if (!elem.valid || steps < 2)
    return track;

  track.reserve(steps);
  for (int i = 0; i < steps; ++i) {
    double jd = jd_start + static_cast<double>(i) *
                               (jd_end - jd_start) /
                               static_cast<double>(steps - 1);
    double lat, lon;
    if (subAsteroidPoint(elem, jd, lat, lon)) {
      track.emplace_back(lat, lon);
    }
  }
  return track;
}
