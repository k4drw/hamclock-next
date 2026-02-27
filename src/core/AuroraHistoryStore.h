#pragma once

#include <chrono>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <vector>

struct AuroraDataPoint {
  float percent;
  std::chrono::system_clock::time_point timestamp;
};

class AuroraHistoryStore {
public:
  static constexpr int MAX_POINTS = 1000; // 24+ hours loosely

  void setStoragePath(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(mutex_);
    storagePath_ = path;
  }

  void addPoint(float percent) {
    std::lock_guard<std::mutex> lock(mutex_);

    AuroraDataPoint point;
    point.percent = percent;
    point.timestamp = std::chrono::system_clock::now();

    history_.push_back(point);

    // Remove points older than 25 hours
    auto now = std::chrono::system_clock::now();
    while (!history_.empty()) {
      auto ageMins = std::chrono::duration_cast<std::chrono::minutes>(
                         now - history_.front().timestamp)
                         .count();
      if (ageMins > 25 * 60) {
        history_.erase(history_.begin());
      } else {
        break;
      }
    }

    // Keep only MAX_POINTS
    while (history_.size() > MAX_POINTS) {
      history_.erase(history_.begin());
    }

    if (!storagePath_.empty()) {
      save_internal();
    }
  }

  void save() {
    std::lock_guard<std::mutex> lock(mutex_);
    save_internal();
  }

  void load() {
    if (storagePath_.empty())
      return;

    try {
      std::ifstream f(storagePath_);
      if (!f.is_open())
        return;

      nlohmann::json j;
      f >> j;

      if (!j.is_array())
        return;

      std::lock_guard<std::mutex> lock(mutex_);
      history_.clear();
      for (const auto &item : j) {
        AuroraDataPoint p;
        p.percent = item["percent"].get<float>();
        p.timestamp = std::chrono::system_clock::from_time_t(
            item["ts"].get<std::time_t>());
        history_.push_back(p);
      }
    } catch (...) {
    }
  }

  std::vector<AuroraDataPoint> getHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
  }

  float getCurrentPercent() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (history_.empty())
      return 0.0f;
    return history_.back().percent;
  }

  bool hasData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !history_.empty();
  }

private:
  void save_internal() {
    if (storagePath_.empty())
      return;

    try {
      nlohmann::json j = nlohmann::json::array();
      for (const auto &p : history_) {
        nlohmann::json point;
        point["percent"] = p.percent;
        point["ts"] = std::chrono::system_clock::to_time_t(p.timestamp);
        j.push_back(point);
      }

      std::ofstream f(storagePath_);
      if (f.is_open()) {
        f << j.dump();
      }
    } catch (...) {
    }
  }

  mutable std::mutex mutex_;
  std::vector<AuroraDataPoint> history_;
  std::filesystem::path storagePath_;
};
