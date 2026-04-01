#pragma once

#include <mutex>
#include <string>
#include <vector>

struct SFIPoint {
  int flux;
  std::string date;  // "YYYY-MM-DD" from time_tag
};

// Store for 30-day SFI history from NOAA 10cm-flux-30-day.json.
// Points are replaced wholesale on each successful fetch.
class SFIHistoryStore {
public:
  void setPoints(std::vector<SFIPoint> pts) {
    std::lock_guard<std::mutex> lk(mutex_);
    points_ = std::move(pts);
  }

  std::vector<SFIPoint> getPoints() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return points_;
  }

  bool hasData() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return !points_.empty();
  }

private:
  mutable std::mutex mutex_;
  std::vector<SFIPoint> points_;
};
