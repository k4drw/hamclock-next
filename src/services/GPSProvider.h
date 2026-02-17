#pragma once

#include "../core/ConfigManager.h"
#include "../core/HamClockState.h"
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace HamClock {

class GPSProvider {
public:
  GPSProvider(HamClockState *state, AppConfig &config);
  ~GPSProvider();

  void start();
  void stop();

private:
  void run();
  void processLine(const std::string &line);

  HamClockState *state_;
  AppConfig &config_;
  std::atomic<bool> stopClicked_{false};
  std::thread thread_;
  std::chrono::steady_clock::time_point lastUpdate_{};
};

} // namespace HamClock
