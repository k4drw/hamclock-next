#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct TidePrediction {
  std::string time; // "HH:MM" local time
  std::string type; // "H" or "L"
  double heightFt = 0.0;
};

struct BuoyObservation {
  std::string stationId;
  double waveHeightM = -1.0;  // Significant wave height (m); -1 = missing
  double wavePeriodS = -1.0;  // Dominant wave period (s)
  double waterTempC = -999.0; // Water temp (°C); -999 = missing
  double windSpeedMps = -1.0; // Wind speed at buoy (m/s)
  int windDirDeg = -1;        // Wind direction (°)
  std::string lastUpdate;
};

struct MarineData {
  std::string tideStationId;
  std::string tideStationName;
  std::vector<TidePrediction> tides; // Next 4-6 high/low tides
  BuoyObservation buoy;
  bool tidesValid = false;
  bool buoyValid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

struct MarineLookupResult {
  std::string closestTideId;
  std::string closestTideName;
  std::string closestBuoyId;
  std::string closestBuoyName;
  double distKmTide = -1.0;
  double distKmBuoy = -1.0;
};

class MarineStore {

public:
  void update(const MarineData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  MarineData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  MarineData data_;
};
