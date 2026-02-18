#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>

// Individual QSO record for display in log viewer
struct QSORecord {
  std::string callsign;
  std::string date; // YYYYMMDD format
  std::string time; // HHMMSS format
  std::string band;
  std::string mode;
  std::string freq;
  std::string rstSent;
  std::string rstRcvd;
  std::string name;
  std::string qth;
  std::string gridsquare;
  std::string comment;
  double lat = 0.0;
  double lon = 0.0;
};

struct ADIFStats {
  int totalQSOs = 0;
  std::map<std::string, int> modeCounts;
  std::map<std::string, int> bandCounts;
  std::vector<std::string> latestCalls;
  std::vector<QSORecord> recentQSOs; // Most recent QSOs (newest first)

  bool valid = false;
  std::string activeBandFilter;
  std::string activeModeFilter;
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

  void setFilters(const std::string &band, const std::string &mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_.activeBandFilter = band;
    stats_.activeModeFilter = mode;
  }

private:
  mutable std::mutex mutex_;
  ADIFStats stats_;
};
