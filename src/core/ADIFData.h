#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

struct ADIFStats {
  int totalQSOs = 0;
  std::map<std::string, int> modeCounts;
  std::map<std::string, int> bandCounts;
  std::vector<std::string> latestCalls;

  bool valid = false;
};

class ADIFStore {
public:
  void update(const ADIFStats &stats) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = stats;
  }

  ADIFStats get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
  }

private:
  mutable std::mutex mutex_;
  ADIFStats stats_;
};
