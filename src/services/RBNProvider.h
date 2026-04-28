#pragma once

#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/HeardMeStore.h"
#include "../core/PrefixManager.h"
#include "../core/WatchlistHitStore.h"
#include "../core/WatchlistStore.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

struct HamClockState;

// Reverse Beacon Network provider.
// Connects to the RBN Telnet feed (telnet.reversebeacon.net:7000), parses
// standard "DX de" spot lines, and feeds spots into the shared
// DXClusterDataStore so they appear in the DX Cluster panel and map overlay.
class RBNProvider {
public:
  explicit RBNProvider(std::shared_ptr<DXClusterDataStore> store,
                       PrefixManager &pm,
                       std::shared_ptr<WatchlistStore> watchlist = nullptr,
                       std::shared_ptr<WatchlistHitStore> hits = nullptr,
                       std::shared_ptr<HeardMeStore> heardMe = nullptr,
                       HamClockState *state = nullptr);
  ~RBNProvider();

  void start(const AppConfig &config);
  void stop();

  bool isRunning() const { return running_; }

private:
  void run();
  void runTelnet(const std::string &host, int port, const std::string &login);
  void processLine(const std::string &line);

  std::shared_ptr<DXClusterDataStore> store_;
  PrefixManager &pm_;
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<WatchlistHitStore> hits_;
  std::shared_ptr<HeardMeStore> heardMe_;
  HamClockState *state_;
  AppConfig config_;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopRequested_{false};
  std::mutex stopMutex_;
  std::condition_variable stopCv_;

  static constexpr const char *DEFAULT_HOST = "telnet.reversebeacon.net";
  static constexpr int DEFAULT_PORT = 7000;
};
