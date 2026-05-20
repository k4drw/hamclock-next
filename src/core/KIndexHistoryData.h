#pragma once

#include <algorithm>
#include <chrono>
#include <mutex>
#include <vector>

struct KIndexPoint {
  float kp;
  std::chrono::system_clock::time_point timestamp;
};

// Ring-buffer store for K-index history.
// Keeps up to 48 hours of readings (one per NOAAProvider poll, ~15-min intervals).
class KIndexHistoryStore {
public:
  static constexpr int MAX_POINTS = 200;

  void push(float kp, std::chrono::system_clock::time_point ts =
                          std::chrono::system_clock::now()) {
    std::lock_guard<std::mutex> lk(mutex_);

    // Check if we already have this exact timestamp (avoid duplicates during
    // backfill)
    for (const auto &p : history_) {
      if (p.timestamp == ts)
        return;
    }

    KIndexPoint p;
    p.kp = kp;
    p.timestamp = ts;
    history_.push_back(p);

    // Keep sorted by timestamp
    std::sort(history_.begin(), history_.end(),
              [](const KIndexPoint &a, const KIndexPoint &b) {
                return a.timestamp < b.timestamp;
              });

    // Prune points older than 48 hours
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(48);
    while (!history_.empty() && history_.front().timestamp < cutoff)
      history_.erase(history_.begin());

    // Hard cap
    while ((int)history_.size() > MAX_POINTS)
      history_.erase(history_.begin());
  }

  std::vector<KIndexPoint> getHistory() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return history_;
  }

  float getCurrent() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return history_.empty() ? 0.0f : history_.back().kp;
  }

  bool hasData() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return !history_.empty();
  }

private:
  mutable std::mutex mutex_;
  std::vector<KIndexPoint> history_;
};
