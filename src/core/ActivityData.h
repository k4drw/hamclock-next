#pragma once

#include <atomic>
#include <chrono>
#include <memory>
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

  bool hasSelection = false;
  ONTASpot selectedSpot;
  
  int activeBandFilter = -1;
  std::string activeModeFilter = "";
};

class ActivityDataStore {
public:
  ActivityDataStore() : data_(std::make_shared<ActivityData>()) {}

  std::shared_ptr<const ActivityData> get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  void set(const ActivityData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = std::make_shared<ActivityData>(data);
    version_++;
  }

  uint32_t getVersion() const { return version_.load(std::memory_order_relaxed); }

private:
  mutable std::mutex mutex_;
  std::shared_ptr<ActivityData> data_;
  std::atomic<uint32_t> version_{0};
};
