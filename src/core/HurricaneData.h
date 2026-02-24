#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct HurricaneStorm {
  std::string id;           // NHC storm ID, e.g. "al012026"
  std::string name;         // "HELENE"
  std::string basin;        // "AL" (Atlantic), "EP" (East Pacific)
  int category = 0;         // 0=TD/TS, 1-5=hurricane, -1=post-tropical
  double lat = 0.0;
  double lon = 0.0;
  int maxWindKt = 0;        // Maximum sustained wind in knots
  int pressureMb = 0;       // Minimum central pressure
  std::string movement;     // e.g. "NNW at 12 mph"
  std::string advisory;     // Short advisory text
  std::string lastUpdate;   // ISO timestamp truncated to 16 chars
};

struct HurricaneData {
  std::vector<HurricaneStorm> storms;
  bool valid = false;
  std::chrono::system_clock::time_point lastUpdate;
};

class HurricaneStore {
public:
  void update(const HurricaneData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  HurricaneData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  HurricaneData data_;
};
