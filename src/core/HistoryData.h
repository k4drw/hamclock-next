#pragma once

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <vector>

struct HistoryPoint {
  std::chrono::system_clock::time_point time;
  float value;

  HistoryPoint() : value(0) {}
  HistoryPoint(std::chrono::system_clock::time_point t, float v)
      : time(t), value(v) {}
};

struct HistorySeries {
  std::string name;
  std::vector<HistoryPoint> points;
  float minValue = 0;
  float maxValue = 0;
  bool valid = false;
};

class HistoryStore {
public:
  void update(const std::string &name, const HistorySeries &series) {
    std::lock_guard<std::mutex> lock(mutex_);
    series_[name] = series;
  }

  HistorySeries get(const std::string &name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = series_.find(name);
    if (it != series_.end())
      return it->second;
    return {};
  }

private:
  mutable std::mutex mutex_;
  std::map<std::string, HistorySeries> series_;
};
