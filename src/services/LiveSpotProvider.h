#pragma once

#include "../core/ConfigManager.h"
#include "../core/DXClusterData.h"
#include "../core/LiveSpotData.h"
#include "../network/NetworkManager.h"

#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/fmt/fmt.h>

struct HamClockState;

class LiveSpotProvider {
public:
  LiveSpotProvider(NetworkManager &net,
                   std::shared_ptr<LiveSpotDataStore> store,
                   const AppConfig &config, HamClockState *state = nullptr,
                   std::shared_ptr<DXClusterDataStore> dxStore = nullptr);

  void fetch();
  void updateConfig(const AppConfig &config) { config_ = config; }
  nlohmann::json getDebugData() const;

private:
  void fetchPSK();
  void fetchWSPR();
  void fetchRBN();

  NetworkManager &net_;
  std::shared_ptr<LiveSpotDataStore> store_;
  std::shared_ptr<DXClusterDataStore> dxStore_;
  AppConfig config_;
  HamClockState *state_;
};
