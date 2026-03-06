#pragma once

#include <chrono>
#include <deque>
#include <mutex>
#include <vector>

struct XRayDataPoint {
  std::chrono::system_clock::time_point timestamp;
  double flux; // W/m²
};

class XRayHistoryStore {
public:
  void push(XRayDataPoint p) {
    std::lock_guard<std::mutex> l(mu_);
    // Remove points older than 6 hours
    auto cutoff = p.timestamp - std::chrono::hours(kMaxHours);
    while (!pts_.empty() && pts_.front().timestamp < cutoff)
      pts_.pop_front();
    pts_.push_back(std::move(p));
  }

  std::vector<XRayDataPoint> getHistory() const {
    std::lock_guard<std::mutex> l(mu_);
    return {pts_.begin(), pts_.end()};
  }

private:
  mutable std::mutex mu_;
  std::deque<XRayDataPoint> pts_;
  static constexpr int kMaxHours = 6;
};
