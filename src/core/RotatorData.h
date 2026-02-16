#pragma once

#include <chrono>
#include <mutex>

// Rotator position and status data
struct RotatorData {
  double azimuth = 0.0;   // Current azimuth (0-360 degrees)
  double elevation = 0.0; // Current elevation (-90 to 90 degrees)
  bool connected = false; // Connection status
  bool moving = false;    // Is rotator currently moving
  std::chrono::system_clock::time_point lastUpdate;
  bool valid = false;
};

// Thread-safe data store for rotator position
class RotatorDataStore {
public:
  RotatorDataStore() = default;

  void set(const RotatorData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  RotatorData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  RotatorData data_;
};
