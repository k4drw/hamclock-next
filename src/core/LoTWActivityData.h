#pragma once

#include <chrono>
#include <mutex>
#include <unordered_map>
#include <string>

// LoTW activity record: callsign -> last upload epoch
struct LoTWActivityRecord {
  std::chrono::system_clock::time_point lastUpload;
};

// Thread-safe store for LoTW user activity data (from lotw.arrl.org/lotw-user-activity.csv)
class LoTWActivityStore {
public:
  void update(std::unordered_map<std::string, std::chrono::system_clock::time_point> activity) {
    std::unordered_map<std::string, std::chrono::system_clock::time_point> old;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      old = std::move(activity_);
      activity_ = std::move(activity);
      lastRefresh_ = std::chrono::system_clock::now();
    }
  }

  // Returns last upload timestamp for a callsign, or epoch(0) if not found
  std::chrono::system_clock::time_point getLastUpload(const std::string &callsign) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = activity_.find(callsign);
    if (it != activity_.end())
      return it->second;
    return std::chrono::system_clock::time_point();
  }

  // Returns true if callsign is known LoTW user
  bool isLoTWUser(const std::string &callsign) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return activity_.find(callsign) != activity_.end();
  }

  // Returns recency category: 0=unknown, 1=<30d, 2=<1yr, 3=>1yr
  int getRecencyCategory(const std::string &callsign) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = activity_.find(callsign);
    if (it == activity_.end())
      return 0; // unknown

    auto now = std::chrono::system_clock::now();
    auto diff = now - it->second;
    auto days = std::chrono::duration_cast<std::chrono::hours>(diff).count() / 24;

    if (days <= 30)
      return 1; // green: recently active
    if (days <= 365)
      return 2; // yellow: somewhat active
    return 3; // grey: inactive
  }

  bool hasData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !activity_.empty();
  }

  std::chrono::system_clock::time_point getLastRefresh() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastRefresh_;
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::chrono::system_clock::time_point> activity_;
  std::chrono::system_clock::time_point lastRefresh_;
};
