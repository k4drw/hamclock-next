#pragma once

#include <chrono>
#include <cmath>
#include <ctime>
#include <string>
#include <vector>

struct SubSolarPoint {
  double lat; // degrees, positive = north
  double lon; // degrees, positive = east [-180, 180]
};

struct TerminatorPoint {
  double lat; // degrees
  double lon; // degrees
};

struct LatLon {
  double lat; // degrees, positive = north
  double lon; // degrees, positive = east
};

struct SunTimes {
  double sunrise; // UTC hours, valid only if hasRise
  double sunset;  // UTC hours, valid only if hasSet
  bool hasRise;
  bool hasSet;
};

class Astronomy {
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kDeg2Rad = kPi / 180.0;
  static constexpr double kRad2Deg = 180.0 / kPi;
  static constexpr double kEarthR = 6371.0; // km

public:
  // Calculate the sub-solar point for a given UTC time.
  static SubSolarPoint sunPosition(std::chrono::system_clock::time_point tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
    gmtime_r(&t, &utc);

    int doy = utc.tm_yday + 1;
    double hours = utc.tm_hour + utc.tm_min / 60.0 + utc.tm_sec / 3600.0;

    constexpr double obliquity = 23.44;
    double decl = -obliquity * std::cos(2.0 * kPi / 365.0 * (doy + 10));

    double B = 2.0 * kPi / 365.0 * (doy - 81);
    double eot_minutes =
        9.87 * std::sin(2.0 * B) - 7.53 * std::cos(B) - 1.5 * std::sin(B);
    double solarHours = hours + eot_minutes / 60.0;
    double lon = -(solarHours - 12.0) * 15.0;

    while (lon > 180.0)
      lon -= 360.0;
    while (lon < -180.0)
      lon += 360.0;

    return {decl, lon};
  }

  // Calculate the terminator (day/night boundary) as a polyline.
  static std::vector<TerminatorPoint>
  calculateTerminator(double sunLat, double sunLon, int numPoints = 361) {
    double sLat = sunLat;
    if (std::fabs(sLat) < 0.01)
      sLat = (sLat >= 0) ? 0.01 : -0.01;
    double tanSunLat = std::tan(sLat * kDeg2Rad);

    std::vector<TerminatorPoint> points;
    points.reserve(numPoints);

    for (int i = 0; i < numPoints; ++i) {
      double lon = -180.0 + 360.0 * i / (numPoints - 1);
      double dlon = lon - sunLon;
      while (dlon > 180.0)
        dlon -= 360.0;
      while (dlon < -180.0)
        dlon += 360.0;

      double lat = std::atan(-std::cos(dlon * kDeg2Rad) / tanSunLat) * kRad2Deg;
      points.push_back({lat, lon});
    }

    return points;
  }

  // --- Geodesic functions ---

  // Haversine distance in km
  static double calculateDistance(LatLon from, LatLon to) {
    double dLat = (to.lat - from.lat) * kDeg2Rad;
    double dLon = (to.lon - from.lon) * kDeg2Rad;
    double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
               std::cos(from.lat * kDeg2Rad) * std::cos(to.lat * kDeg2Rad) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return kEarthR * c;
  }

  // Initial bearing (forward azimuth) in degrees [0, 360)
  static double calculateBearing(LatLon from, LatLon to) {
    double lat1 = from.lat * kDeg2Rad;
    double lat2 = to.lat * kDeg2Rad;
    double dLon = (to.lon - from.lon) * kDeg2Rad;
    double y = std::sin(dLon) * std::cos(lat2);
    double x = std::cos(lat1) * std::sin(lat2) -
               std::sin(lat1) * std::cos(lat2) * std::cos(dLon);
    double bearing = std::atan2(y, x) * kRad2Deg;
    return std::fmod(bearing + 360.0, 360.0);
  }

  // Great circle path as N intermediate points
  static std::vector<LatLon> calculateGreatCirclePath(LatLon from, LatLon to,
                                                      int numPoints = 100) {
    double lat1 = from.lat * kDeg2Rad;
    double lon1 = from.lon * kDeg2Rad;
    double lat2 = to.lat * kDeg2Rad;
    double lon2 = to.lon * kDeg2Rad;

    // Angular distance
    double d =
        2.0 *
        std::asin(std::sqrt(std::pow(std::sin((lat1 - lat2) / 2), 2) +
                            std::cos(lat1) * std::cos(lat2) *
                                std::pow(std::sin((lon1 - lon2) / 2), 2)));

    std::vector<LatLon> path;
    path.reserve(numPoints);

    if (d < 1e-10) {
      path.push_back(from);
      return path;
    }

    for (int i = 0; i < numPoints; ++i) {
      double f = static_cast<double>(i) / (numPoints - 1);
      double A = std::sin((1.0 - f) * d) / std::sin(d);
      double B = std::sin(f * d) / std::sin(d);
      double x = A * std::cos(lat1) * std::cos(lon1) +
                 B * std::cos(lat2) * std::cos(lon2);
      double y = A * std::cos(lat1) * std::sin(lon1) +
                 B * std::cos(lat2) * std::sin(lon2);
      double z = A * std::sin(lat1) + B * std::sin(lat2);
      double lat = std::atan2(z, std::sqrt(x * x + y * y)) * kRad2Deg;
      double lon = std::atan2(y, x) * kRad2Deg;
      path.push_back({lat, lon});
    }

    return path;
  }

  // --- Sunrise / Sunset ---

  static SunTimes calculateSunTimes(double lat, double lon, int doy) {
    double decl = -23.44 * std::cos(2.0 * kPi / 365.0 * (doy + 10));
    double declRad = decl * kDeg2Rad;
    double latRad = lat * kDeg2Rad;

    // Hour angle when sun center is 0.833° below horizon (refraction
    // correction)
    double cosHA =
        (std::sin(-0.833 * kDeg2Rad) - std::sin(latRad) * std::sin(declRad)) /
        (std::cos(latRad) * std::cos(declRad));

    SunTimes times{};
    if (cosHA > 1.0) {
      // Polar night — sun never rises
      times.hasRise = false;
      times.hasSet = false;
      return times;
    }
    if (cosHA < -1.0) {
      // Midnight sun — sun never sets
      times.hasRise = false;
      times.hasSet = false;
      return times;
    }

    double HA = std::acos(cosHA) * kRad2Deg; // degrees

    // Equation of time
    double B = 2.0 * kPi / 365.0 * (doy - 81);
    double eot =
        9.87 * std::sin(2.0 * B) - 7.53 * std::cos(B) - 1.5 * std::sin(B);

    // Solar noon in UTC hours
    double solarNoon = 12.0 - lon / 15.0 - eot / 60.0;

    times.sunrise = solarNoon - HA / 15.0;
    times.sunset = solarNoon + HA / 15.0;
    times.hasRise = true;
    times.hasSet = true;

    // Normalize to [0, 24)
    auto norm24 = [](double &h) {
      while (h < 0.0)
        h += 24.0;
      while (h >= 24.0)
        h -= 24.0;
    };
    norm24(times.sunrise);
    norm24(times.sunset);

    return times;
  }

  // --- Grid square conversion ---

  // Maidenhead grid to lat/lon (center of square)
  static bool gridToLatLon(const std::string &grid, double &lat, double &lon) {
    if (grid.size() < 4)
      return false;

    // First two: Field (A-R), case-insensitive
    auto toUpper = [](char c) -> char {
      return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
    };
    char f1 = toUpper(grid[0]);
    char f2 = toUpper(grid[1]);
    char s1 = grid[2], s2 = grid[3];

    if (f1 < 'A' || f1 > 'R' || f2 < 'A' || f2 > 'R')
      return false;
    if (s1 < '0' || s1 > '9' || s2 < '0' || s2 > '9')
      return false;

    lon = (f1 - 'A') * 20.0 + (s1 - '0') * 2.0 - 180.0;
    lat = (f2 - 'A') * 10.0 + (s2 - '0') * 1.0 - 90.0;

    if (grid.size() >= 6) {
      // 5th and 6th: Subsquares (a-x), case-insensitive
      auto toLower = [](char c) -> char {
        return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
      };
      char t1 = toLower(grid[4]);
      char t2 = toLower(grid[5]);
      if (t1 >= 'a' && t1 <= 'x' && t2 >= 'a' && t2 <= 'x') {
        lon += (t1 - 'a') * (2.0 / 24.0) + (1.0 / 24.0);
        lat += (t2 - 'a') * (1.0 / 24.0) + (0.5 / 24.0);
      } else {
        // Fallback to center of 2x2 square if subsquares are invalid
        lon += 1.0;
        lat += 0.5;
      }
    } else {
      // Center of 2x2 degree square
      lon += 1.0;
      lat += 0.5;
    }
    return true;
  }

  // Lat/lon to 6-character Maidenhead grid
  static std::string latLonToGrid(double lat, double lon) {
    double lo = lon + 180.0;
    double la = lat + 90.0;
    // Clamp to valid range
    if (lo < 0)
      lo = 0;
    if (lo >= 360.0)
      lo = 359.999;
    if (la < 0)
      la = 0;
    if (la >= 180.0)
      la = 179.999;

    int fLon = static_cast<int>(lo / 20.0);
    int fLat = static_cast<int>(la / 10.0);
    int sLon = static_cast<int>(std::fmod(lo, 20.0) / 2.0);
    int sLat = static_cast<int>(std::fmod(la, 10.0));
    int tLon = static_cast<int>(std::fmod(lo, 2.0) / (2.0 / 24.0));
    int tLat = static_cast<int>(std::fmod(la, 1.0) / (1.0 / 24.0));

    char grid[7];
    grid[0] = static_cast<char>('A' + fLon);
    grid[1] = static_cast<char>('A' + fLat);
    grid[2] = static_cast<char>('0' + sLon);
    grid[3] = static_cast<char>('0' + sLat);
    grid[4] = static_cast<char>('a' + tLon);
    grid[5] = static_cast<char>('a' + tLat);
    grid[6] = '\0';
    return grid;
  }

  // Calculate Azimuth and Elevation for an observer at stationLoc
  // given the sub-solar point (sunPos).
  static void calculateAzEl(LatLon stationLoc, SubSolarPoint sunPos, double &az,
                            double &el) {
    double phi = stationLoc.lat * kDeg2Rad;
    double lam = stationLoc.lon * kDeg2Rad;
    double delta = sunPos.lat * kDeg2Rad;
    double lamS = sunPos.lon * kDeg2Rad;

    double ha = lam - lamS; // Hour angle
    while (ha > kPi)
      ha -= 2.0 * kPi;
    while (ha < -kPi)
      ha += 2.0 * kPi;

    // Elevation
    el = std::asin(std::sin(phi) * std::sin(delta) +
                   std::cos(phi) * std::cos(delta) * std::cos(ha));

    // Azimuth
    double y = std::sin(ha);
    double x = std::cos(ha) * std::sin(phi) - std::tan(delta) * std::cos(phi);
    az = std::atan2(y, x) + kPi; // 0 is North, clockwise
    az = std::fmod(az * kRad2Deg + 360.0, 360.0);
    el *= kRad2Deg;
  }
};
