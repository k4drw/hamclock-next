#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct ForecastPeriod {
  std::string name;           // "Tonight", "Monday", "Monday Night", ...
  std::string shortForecast;  // "Partly Cloudy"
  int tempF = 0;              // Temperature in Fahrenheit (NWS default)
  bool isDaytime = true;
  int precipitationPercent = 0; // Probability of precipitation (%)
  std::string windSpeed;        // "5 to 10 mph"
  std::string windDirection;    // "SW"
};

struct ForecastData {
  std::vector<ForecastPeriod> periods; // Up to 14 periods (7 days × 2)
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class ForecastStore {
public:
  void update(const ForecastData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  ForecastData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  ForecastData data_;
};
