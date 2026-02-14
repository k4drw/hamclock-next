#pragma once

#include <chrono>
#include <cstdlib>
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
};

class DXClusterDataStore {
public:
  DXClusterDataStore();
  ~DXClusterDataStore();

  DXClusterData get() const;
  void set(const DXClusterData &data);
  void addSpot(const DXClusterSpot &spot);
  void setConnected(bool connected, const std::string &status = "");
  void clear();

  void selectSpot(const DXClusterSpot &spot);
  void clearSelection();

  // Load persisted spots from DB.
  void loadPersisted();

private:
  void pruneOldSpots();
  // We'll keep DB interaction strictly inside implementation for now.

  mutable std::mutex mutex_;
  DXClusterData data_;
};
