#pragma once

#include <chrono>
#include <memory>
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

  bool hasSelection = false;
  DXClusterSpot selectedSpot;

  std::string watchedCall;
  bool watchedSpotted = false;
  std::chrono::steady_clock::time_point watchedSpottedAt;
};

class DXClusterDataStore {
public:
  DXClusterDataStore();
  ~DXClusterDataStore();

  std::shared_ptr<const DXClusterData> snapshot() const;
  void addSpot(const DXClusterSpot &spot);
  void setConnected(bool connected, const std::string &status = "");
  void setMaxAgeMinutes(int minutes);
  void clear();

  void setSpots(const std::vector<DXClusterSpot> &spots);

  void selectSpot(const DXClusterSpot &spot);
  void clearSelection();

  void setWatchedCall(const std::string &call);
  void setWatchedSpotted(std::chrono::steady_clock::time_point when);
  void clearWatchedSpotted();

  // Prune in-memory spots older than maxAgeMinutes — call periodically even
  // when no new spots arrive (telnet dropped, rate-limited, etc.)
  void pruneInMemory();

  // Load persisted spots from DB.
  void loadPersisted();

private:
  void pruneOldSpots();
  // We'll keep DB interaction strictly inside implementation for now.

  mutable std::mutex mutex_;
  std::shared_ptr<DXClusterData> data_;
  int maxAgeMinutes_ = 20;
};
