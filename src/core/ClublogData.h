#pragma once

#include <mutex>
#include <vector>
#include <string>

struct MostWantedEntry {
  int rank;
  int dxcc;
  std::string name;
  std::string prefix;
};

class ClublogStore {
public:
  void update(const std::vector<MostWantedEntry> &entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_ = entries;
  }

  std::vector<MostWantedEntry> get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
  }

  bool hasData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !entries_.empty();
  }

private:
  mutable std::mutex mutex_;
  std::vector<MostWantedEntry> entries_;
};
