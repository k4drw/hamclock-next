#pragma once

#include "../core/ConfigManager.h"
#include "../core/LiveSpotData.h"
#include "../network/NetworkManager.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>
#include <string>

struct HamClockState;

class LiveSpotProvider {
public:
  LiveSpotProvider(NetworkManager &net,
                   std::shared_ptr<LiveSpotDataStore> store,
                   const AppConfig &config, HamClockState *state = nullptr);

  void fetch();
  void updateConfig(const AppConfig &config) { config_ = config; }
  nlohmann::json getDebugData() const;

private:
  NetworkManager &net_;
  std::shared_ptr<LiveSpotDataStore> store_;
  AppConfig config_;
  HamClockState *state_;
};
