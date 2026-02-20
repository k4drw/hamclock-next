#pragma once

#include "../network/NetworkManager.h"
#include "OrbitPredictor.h"
#include "SatelliteTypes.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class RotatorService; // Forward declaration

// High-level abstraction for a satellite
class Satellite {
public:
  Satellite(const SatelliteTLE &tle) : tle_(tle) { predictor_.loadTLE(tle); }
  void setObserver(double lat, double lon) { predictor_.setObserver(lat, lon); }
  SatObservation predict(std::time_t now = 0) const {
    if (now == 0)
      now = std::time(nullptr);
    return predictor_.observeAt(now);
  }
  const std::string &getName() const { return tle_.name; }
  const SatelliteTLE &getTLE() const { return tle_; }

private:
  SatelliteTLE tle_;
  mutable OrbitPredictor predictor_;
};

class SatelliteManager {
public:
  explicit SatelliteManager(NetworkManager &net);
  void fetch(bool force = false);

  // Deprecated: Tracking logic moved to RotatorService
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
  std::unique_ptr<Satellite> currentSat_;
  double obsLat_ = 0.0;
  double obsLon_ = 0.0;
};
