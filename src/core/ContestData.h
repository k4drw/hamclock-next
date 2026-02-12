#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct Contest {
  std::string title;
  std::chrono::system_clock::time_point startTime;
  std::chrono::system_clock::time_point endTime;
  std::string url;
};

struct ContestData {
  std::vector<Contest> contests;
  std::chrono::system_clock::time_point lastUpdate;
  bool valid = false;
};

class ContestStore {
public:
  void update(const ContestData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  ContestData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

private:
  mutable std::mutex mutex_;
  ContestData data_;
};
