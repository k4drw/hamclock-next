#pragma once

#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/PrefixManager.h"
#include "../core/WatchlistHitStore.h"
#include "../core/WatchlistStore.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

struct HamClockState;

class DXClusterProvider {
public:
  explicit DXClusterProvider(
      std::shared_ptr<DXClusterDataStore> store, PrefixManager &pm,
      std::shared_ptr<WatchlistStore> watchlist = nullptr,
      std::shared_ptr<WatchlistHitStore> hits = nullptr,
      HamClockState *state = nullptr);
  ~DXClusterProvider();

  void start(const AppConfig &config);
  void stop();

  bool isRunning() const { return running_; }
  nlohmann::json getDebugData() const;

private:
  void run();
  void runHubClient();
  void runTelnet(const std::string &host, int port, const std::string &login);
  void runUDP(int port);

  void processLine(const std::string &line);
  void processWSJTX(const uint8_t *packet, size_t len);

  // Connection rate limiting
  bool checkConnectionRate();
  void incrementConnectionAttempts();
  int connectionAttempts_ = 0;
  std::chrono::system_clock::time_point firstAttemptTime_;

  std::shared_ptr<DXClusterDataStore> store_;
  PrefixManager &pm_;
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<WatchlistHitStore> hits_;
  AppConfig config_;
  HamClockState *state_;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopClicked_{false};
  std::mutex sleepMutex_;
  std::condition_variable sleepCv_;
};
