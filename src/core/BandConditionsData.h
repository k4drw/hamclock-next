#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

enum class BandCondition { POOR, FAIR, GOOD, EXCELLENT, UNKNOWN };

struct BandStatus {
  std::string band;
  BandCondition day;
  BandCondition night;
};

struct BandConditionsData {
  std::vector<BandStatus> statuses;
  std::chrono::system_clock::time_point lastUpdate;
  bool valid = false;
};

class BandConditionsStore {
public:
  void update(const BandConditionsData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  BandConditionsData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  BandConditionsData data_;
};
