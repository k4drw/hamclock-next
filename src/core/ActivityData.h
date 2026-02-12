#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct DXPedition {
  std::string call;
  std::string location;
  std::chrono::system_clock::time_point startTime;
  std::chrono::system_clock::time_point endTime;
  double lat = 0;
  double lon = 0;
};

struct ONTASpot {
  std::string call;
  std::string program; // POTA, SOTA, etc.
  std::string ref;     // e.g. K-1234
  double freqKhz = 0;
  std::string mode;
  std::chrono::system_clock::time_point spottedAt;
  double lat = 0;
  double lon = 0;
};

struct ActivityData {
  std::vector<DXPedition> dxpeds;
  std::vector<ONTASpot> ontaSpots;
  std::chrono::system_clock::time_point lastUpdated;
  bool valid = false;
};

class ActivityDataStore {
public:
  ActivityData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  void set(const ActivityData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

private:
  mutable std::mutex mutex_;
  ActivityData data_;
};
