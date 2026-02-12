#pragma once

#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include <atomic>
#include <memory>
#include <string>
#include <thread>

class DXClusterProvider {
public:
  explicit DXClusterProvider(std::shared_ptr<DXClusterDataStore> store);
  ~DXClusterProvider();

  void start(const AppConfig &config);
  void stop();

  bool isRunning() const { return running_; }

private:
  void run();
  void runTelnet(const std::string &host, int port, const std::string &login);
  void runUDP(int port);

  void processLine(const std::string &line);

  std::shared_ptr<DXClusterDataStore> store_;
  AppConfig config_;

  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> stopClicked_{false};
};
