#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct RepeaterEntry {
  std::string callsign;
  double freqMHz = 0.0;    // Output frequency in MHz
  double offsetMHz = 0.0;  // Offset in MHz (+/-)
  std::string tone;         // CTCSS tone or "none"
  std::string mode;         // "FM", "DMR", "Fusion", "D-STAR", "P25"
  std::string use;          // "OPEN", "CLOSED", "PRIVATE"
  std::string state;
  std::string county;
  double distanceKm = 0.0; // Distance from DE (filled by provider)
};

struct RepeaterData {
  std::vector<RepeaterEntry> repeaters;
  bool valid = false;
  bool fetched = false; // true once at least one fetch attempt completed
  std::chrono::system_clock::time_point lastUpdate;
};

class RepeaterStore {
public:
  void update(const RepeaterData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  RepeaterData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  RepeaterData data_;
};
