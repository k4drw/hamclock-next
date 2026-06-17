#pragma once

#include "../core/PowerwallStore.h"
#include "../network/NetworkManager.h"
#include "../core/ConfigManager.h"
#include <memory>
#include <string>
#include <atomic>
#include <chrono>

class PowerwallProvider {
public:
    PowerwallProvider(NetworkManager& net, std::shared_ptr<PowerwallStore> store, AppConfig& config);
    ~PowerwallProvider();

    void fetch();

private:
    NetworkManager& net_;
    std::shared_ptr<PowerwallStore> store_;
    AppConfig& config_;
    
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    std::chrono::steady_clock::time_point lastFetchTime_;
    bool isFetching_ = false;
};
