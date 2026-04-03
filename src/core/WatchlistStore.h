#pragma once

#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct WatchlistData {
  std::set<std::string> calls;
};

class WatchlistStore {
public:
  void add(const std::string &call) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string c = call;
    std::transform(c.begin(), c.end(), c.begin(), ::toupper);
    data_.calls.insert(c);
  }

  void remove(const std::string &call) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string c = call;
    std::transform(c.begin(), c.end(), c.begin(), ::toupper);
    data_.calls.erase(c);
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.calls.clear();
  }

  bool contains(const std::string &call) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string c = call;
    // Handle suffix like K1ABC/P
    size_t slash = c.find('/');
    if (slash != std::string::npos)
      c = c.substr(0, slash);
    std::transform(c.begin(), c.end(), c.begin(), ::toupper);
    return data_.calls.count(c) > 0;
  }

  std::vector<std::string> getAll() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<std::string>(data_.calls.begin(), data_.calls.end());
  }

private:
  mutable std::mutex mutex_;
  WatchlistData data_;
};
