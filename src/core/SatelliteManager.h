#pragma once

#include "../services/ProviderBase.h"
#include "../network/NetworkManager.h"
#include "OrbitPredictor.h"
#include "SatelliteTypes.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
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

class SatelliteManager : public ProviderBase {
public:
  explicit SatelliteManager(NetworkManager &net);
  ~SatelliteManager();
  void fetch(bool force = false);

  // Called when TLE string is fetched from celestrak.
  void onDataReady(const std::string &raw, int type = 0);

  // Deprecated: Tracking logic moved to RotatorService
  void update();

  std::vector<SatelliteTLE> getSatellites() const;
  bool hasData() const;
  std::optional<SatelliteTLE> findByNoradId(int noradId) const;
  std::optional<SatelliteTLE> findByName(const std::string &search) const;

  void setRotatorService(RotatorService *rotator);
  void trackSatellite(const std::string &satName);
  std::string getTrackedSatellite() const;

  // Custom satellite management
  const std::vector<SatelliteTLE> &getCustomSatellites() const {
    return customSatellites_;
  }

  void addCustomSCC(int noradId);
  void removeCustomSCC(int noradId);
  void addCustomTLE(const SatelliteTLE &tle);
  void removeCustomTLE(const std::string &satName);

  void setObserver(double lat, double lon) {
    obsLat_ = lat;
    obsLon_ = lon;
  }

private:
  // Parse the multi-line TLE response.
  void parse(const std::string &raw);
  void parseRecent(const std::string &raw);

  static constexpr const char *TLE_URL =
      "https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle";
  static constexpr const char *TLE_LAST_30_URL =
      "https://celestrak.org/NORAD/elements/gp.php?GROUP=last-30-days&FORMAT=tle";
  static constexpr const char *SCC_URL_TEMPLATE =
      "https://celestrak.org/NORAD/elements/gp.php?CATNR={}&FORMAT=tle";

  void fetchSCC(int noradId);
  void loadLocalTLEs();
  void saveLocalTLEs();
  std::filesystem::path getLocalTLEPath() const;

  NetworkManager &net_;
  RotatorService *rotator_ = nullptr;

  mutable std::mutex mutex_;
  std::vector<SatelliteTLE> satellites_;       // Global list (amateur)
  std::vector<SatelliteTLE> recentSatellites_; // Last 30 days
  std::vector<SatelliteTLE> customSatellites_; // Custom SCCs and uploads
  bool dataValid_ = false;
  std::chrono::steady_clock::time_point lastFetch_;

  std::string trackedSatName_;
  std::unique_ptr<Satellite> currentSat_;
  double obsLat_ = 0.0;
  double obsLon_ = 0.0;
};
