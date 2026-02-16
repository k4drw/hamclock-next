#pragma once

#include <mutex>
#include <string>
#include <vector>

struct LatLong {
  double lat;
  double lon;

  void normalize() {
    // Basic normalization if needed, similar to original hamclock
    while (lat > 90)
      lat -= 180;
    while (lat < -90)
      lat += 180;
    while (lon > 180)
      lon -= 360;
    while (lon < -180)
      lon += 360;
  }
};

class PrefixManager {
public:
  PrefixManager();

  // Initialize: load from static data
  void init();

  // Find location for a callsign. Returns true if found.
  // Thread-safe.
  bool findLocation(const std::string &call, LatLong &ll);

  // Find DXCC entity number for a callsign. Returns -1 if not found.
  int findDXCC(const std::string &call);

  // Get country name from DXCC number. Returns empty string if not found.
  std::string getCountryName(int dxcc);

  // Get continent from DXCC number. Returns empty string if not found.
  std::string getContinent(int dxcc);

  // Get CQ zone from DXCC number. Returns -1 if not found.
  int getCQZone(int dxcc);

  // Get ITU zone from DXCC number. Returns -1 if not found.
  int getITUZone(int dxcc);

  // Helper structure for internal storage
  struct PrefixEntry {
    std::string call;
    float lat;
    float lon;
    int dxcc;
  };

private:
  std::vector<PrefixEntry> entries_;
  std::mutex mutex_;
};
