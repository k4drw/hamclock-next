#pragma once

#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <string>
#include <vector>

struct CalendarEvent {
  std::string summary;
  std::string source;
  time_t start = 0;
  time_t end = 0;
  bool allDay = false; // true if duration >= 23h and start is at midnight
};

class CalendarStore {
public:
  void setCachePath(std::filesystem::path p) {
    std::lock_guard<std::mutex> lk(mutex_);
    cachePath_ = std::move(p);
  }

  // Load non-expired events from the cache file. Call once at startup.
  void loadCache() {
    std::lock_guard<std::mutex> lk(mutex_);
    if (cachePath_.empty()) return;
    std::ifstream f(cachePath_);
    if (!f.is_open()) return;
    try {
      auto arr = nlohmann::json::parse(f);
      if (!arr.is_array()) return;
      time_t now = std::time(nullptr);
      std::vector<CalendarEvent> loaded;
      for (const auto &item : arr) {
        CalendarEvent ev;
        ev.summary = item.value("summary", std::string{});
        ev.source  = item.value("source",  std::string{});
        ev.start   = (time_t)item.value("start",  (int64_t)0);
        ev.end     = (time_t)item.value("end",    (int64_t)0);
        ev.allDay  = item.value("allDay",  false);
        if (ev.end > now)
          loaded.push_back(std::move(ev));
      }
      events_ = std::move(loaded);
    } catch (...) {}
  }

  void set(std::vector<CalendarEvent> events) {
    std::lock_guard<std::mutex> lk(mutex_);
    events_ = std::move(events);
    notifiedKeys_.clear();
    saveCache_locked();
  }

  std::vector<CalendarEvent> snapshot() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return events_;
  }

  // Mark an event (by start epoch) as already notified. Returns true if it
  // was not yet marked (first notification).
  bool markNotified(time_t startEpoch) {
    std::lock_guard<std::mutex> lk(mutex_);
    if (notifiedKeys_.count((uint64_t)startEpoch))
      return false;
    notifiedKeys_.insert((uint64_t)startEpoch);
    return true;
  }

private:
  void saveCache_locked() {
    if (cachePath_.empty()) return;
    try {
      auto arr = nlohmann::json::array();
      for (const auto &ev : events_) {
        nlohmann::json je;
        je["summary"] = ev.summary;
        je["source"]  = ev.source;
        je["start"]   = (int64_t)ev.start;
        je["end"]     = (int64_t)ev.end;
        je["allDay"]  = ev.allDay;
        arr.push_back(je);
      }
      std::ofstream f(cachePath_);
      f << arr.dump(2);
    } catch (...) {}
  }

  mutable std::mutex mutex_;
  std::vector<CalendarEvent> events_;
  std::unordered_set<uint64_t> notifiedKeys_;
  std::filesystem::path cachePath_;
};
