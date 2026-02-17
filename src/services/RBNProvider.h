#pragma once

#include "../core/ConfigManager.h"
#include "../core/LiveSpotData.h"
#include "../core/PrefixManager.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

struct HamClockState;

// Reverse Beacon Network provider.
// Connects to the RBN Telnet feed (telnet.reversebeacon.net:7000), parses
// standard "DX de" spot lines, and feeds spots into the shared
// LiveSpotDataStore for real-time activity visualization.
class RBNProvider {
public:
  explicit RBNProvider(std::shared_ptr<LiveSpotDataStore> store,
                       PrefixManager &pm, HamClockState *state = nullptr);
  ~RBNProvider();

  void start(const AppConfig &config);
  void stop();

  bool isRunning() const { return running_; }

private:
  void run();
  void runTelnet(const std::string &host, int port, const std::string &login);
  void processLine(const std::string &line);

  std::shared_ptr<LiveSpotDataStore> store_;
  PrefixManager &pm_;
  HamClockState *state_;
  AppConfig config_;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopRequested_{false};

  static constexpr const char *DEFAULT_HOST = "telnet.reversebeacon.net";
  static constexpr int DEFAULT_PORT = 7000;
};
