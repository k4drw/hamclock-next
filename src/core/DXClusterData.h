#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct DXClusterSpot {
  std::string txCall;
  std::string txGrid;
  std::string rxCall;
  std::string rxGrid;

  int txDxcc = 0;
  int rxDxcc = 0;

  std::string mode;
  double freqKhz = 0.0;
  double snr = 0.0;

  double txLat = 0.0;
  double txLon = 0.0;
  double rxLat = 0.0;
  double rxLon = 0.0;

  std::chrono::system_clock::time_point spottedAt;
};

struct DXClusterData {
  std::vector<DXClusterSpot> spots;
  bool connected = false;
  std::string statusMsg;
  std::chrono::system_clock::time_point lastUpdate;
};

class DXClusterDataStore {
public:
  DXClusterData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  void set(const DXClusterData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

  void addSpot(const DXClusterSpot &spot) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Add unique spot (same call and band/freq?)
    // Similar to original addDXClusterSpot logic

    // For now, just append and prune old ones (e.g. older than 60 mins)
    data_.spots.push_back(spot);
    pruneOldSpots();
    data_.lastUpdate = std::chrono::system_clock::now();
  }

  void setConnected(bool connected, const std::string &status = "") {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.connected = connected;
    data_.statusMsg = status;
    data_.lastUpdate = std::chrono::system_clock::now();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.spots.clear();
    data_.lastUpdate = std::chrono::system_clock::now();
  }

private:
  void pruneOldSpots() {
    auto now = std::chrono::system_clock::now();
    auto maxAge = std::chrono::minutes(60); // Default 60 mins

    data_.spots.erase(std::remove_if(data_.spots.begin(), data_.spots.end(),
                                     [&](const DXClusterSpot &s) {
                                       return (now - s.spottedAt) > maxAge;
                                     }),
                      data_.spots.end());
  }

  mutable std::mutex mutex_;
  DXClusterData data_;
};
