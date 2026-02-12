#pragma once

#include <chrono>
#include <mutex>
#include <string>

struct WeatherData {
  float temp = 0;      // Celsius
  float pressure = 0;  // hPa
  int humidity = 0;    // %
  float windSpeed = 0; // m/s
  int windDeg = 0;
  std::string description;
  std::string icon; // OWM icon code or similar
  std::string city;

  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class WeatherStore {
public:
  void update(const WeatherData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  WeatherData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  WeatherData data_;
};
