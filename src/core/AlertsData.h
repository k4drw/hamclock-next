#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct WeatherAlert {
  std::string id;
  std::string event;    // e.g. "Tornado Warning"
  std::string severity; // "Extreme", "Severe", "Moderate", "Minor", "Unknown"
  std::string headline;
  std::string expires; // ISO time, truncated to 16 chars
};

struct AlertsData {
  std::vector<WeatherAlert> alerts;
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class AlertsStore {
public:
  void update(const AlertsData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  AlertsData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  AlertsData data_;
};
