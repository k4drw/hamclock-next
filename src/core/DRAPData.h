#pragma once

#include <chrono>
#include <mutex>
#include <vector>

struct DRAPGrid {
  std::vector<float> cells; // row-major [lat_idx * cols + lon_idx], MHz
  int rows = 0;
  int cols = 0;
  bool valid = false;
  std::chrono::system_clock::time_point updated;
};

class DRAPDataStore {
public:
  DRAPGrid get() const {
    std::lock_guard<std::mutex> l(mu_);
    return grid_;
  }
  void set(DRAPGrid g) {
    std::lock_guard<std::mutex> l(mu_);
    grid_ = std::move(g);
  }

private:
  mutable std::mutex mu_;
  DRAPGrid grid_;
};
