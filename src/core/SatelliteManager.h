#pragma once

#include "../network/NetworkManager.h"
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

class RotatorService; // Forward declaration

struct SatelliteTLE {
  std::string name;  // Satellite name (trimmed)
  std::string line1; // TLE line 1
  std::string line2; // TLE line 2
  int noradId = 0;   // NORAD catalog number (from line 1)
};

class SatelliteManager {
public:
  explicit SatelliteManager(NetworkManager &net);
  void fetch(bool force = false);
  void update();
  std::vector<SatelliteTLE> getSatellites() const;
  bool hasData() const;
  const SatelliteTLE *findByNoradId(int noradId) const;
  const SatelliteTLE *findByName(const std::string &search) const;

  void setRotatorService(RotatorService *rotator) { rotator_ = rotator; }
  void trackSatellite(const std::string &satName);
  std::string getTrackedSatellite() const;

  void setObserver(double lat, double lon) {
    obsLat_ = lat;
    obsLon_ = lon;
  }

private:
  void parse(const std::string &raw);

  static constexpr const char *TLE_URL =
      "https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle";

  NetworkManager &net_;
  RotatorService *rotator_ = nullptr;

  mutable std::mutex mutex_;
  std::vector<SatelliteTLE> satellites_;
  bool dataValid_ = false;
  std::chrono::steady_clock::time_point lastFetch_;

  std::string trackedSatName_;
  double obsLat_ = 0.0;
  double obsLon_ = 0.0;
};

