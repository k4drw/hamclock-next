#pragma once

#include <chrono>
#include <mutex>
#include <vector>

struct AuroraMapData {
  // OVATION grid is 360 (lon) x 181 (lat)
  // Lat: -90 to 90, Lon: 0 to 359
  std::vector<uint8_t> grid;
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;

  AuroraMapData() { grid.resize(360 * 181, 0); }
};

class AuroraMapStore {
public:
  void update(const AuroraMapData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  AuroraMapData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  AuroraMapData data_;
};
