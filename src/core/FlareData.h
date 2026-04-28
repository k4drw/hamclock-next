#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <ctime>

struct FlareEvent {
  std::string class_type;  // "A", "B", "C", "M", "X"
  int peak_time_utc = 0;   // Time in minutes since 0000 UTC
  int begin_time_utc = 0;
  int end_time_utc = 0;
  time_t last_updated = 0;

  // Helper to get hh:mm format for UTC time
  std::string peakTimeString() const {
    int hh = peak_time_utc / 60;
    int mm = peak_time_utc % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    return std::string(buf);
  }

  std::string durationString() const {
    int begin_hh = begin_time_utc / 60;
    int begin_mm = begin_time_utc % 60;
    int end_hh = end_time_utc / 60;
    int end_mm = end_time_utc % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d–%02d:%02d", begin_hh, begin_mm,
             end_hh, end_mm);
    return std::string(buf);
  }
};

struct FlareData {
  std::vector<FlareEvent> events;
  time_t last_fetch = 0;
  bool valid = false;
};

class FlareDataStore {
public:
  FlareData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  void set(const FlareData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

private:
  mutable std::mutex mutex_;
  FlareData data_;
};
