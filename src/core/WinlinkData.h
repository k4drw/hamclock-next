#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct WinlinkGateway {
  std::string callsign;
  double lat = 0.0;
  double lon = 0.0;
  double distanceKm = 0.0;
  std::string modes;         // "VHF,HF" etc.
  double freqMHz = 0.0;     // Primary frequency
  std::string lastHeard;    // ISO time truncated
  int hoursSinceHeard = -1; // Convenience field
};

struct WinlinkData {
  std::vector<WinlinkGateway> gateways;
  bool valid = false;
  bool fetched = false; // true once at least one fetch attempt completed
  std::chrono::system_clock::time_point lastUpdate;
};

class WinlinkStore {
public:
  void update(const WinlinkData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  WinlinkData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  WinlinkData data_;
};
