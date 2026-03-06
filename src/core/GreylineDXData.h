#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct GreylineEntity {
  std::string name;
  std::string prefix;
  double lat;
  double lon;
  double minutesToPeak; // negative = peak passed, positive = peak approaching
  bool isSunrise;       // true = sunrise, false = sunset
};

struct GreylineDXData {
  std::vector<GreylineEntity> activeEntities;
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class GreylineDXStore {
public:
  void update(const GreylineDXData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  GreylineDXData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  GreylineDXData data_;
};
