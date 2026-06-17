#include "PowerwallProvider.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PowerwallProvider::PowerwallProvider(NetworkManager& net, std::shared_ptr<PowerwallStore> store, AppConfig& config)
    : net_(net), store_(std::move(store)), config_(config) {}

PowerwallProvider::~PowerwallProvider() {
    alive_->store(false, std::memory_order_release);
}

void PowerwallProvider::fetch() {
    if (config_.powerwallUrl.empty()) return;
    
    int interval = config_.powerwallPollInterval;
    if (interval < 5) interval = 5;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFetchTime_).count();
    
    if (elapsed >= interval && !isFetching_) {
        isFetching_ = true;
        lastFetchTime_ = now;
        
        std::string aggUrl = config_.powerwallUrl + "/api/meters/aggregates";
        std::string soeUrl = config_.powerwallUrl + "/api/system_status/soe";
        
        auto alive = alive_;
        
        // Fetch Aggregates first
        net_.fetchAsync(aggUrl, [this, alive, soeUrl](std::string response) {
            if (!alive->load(std::memory_order_acquire)) return;
            
            PowerwallData newData;
            if (!response.empty()) {
                try {
                    auto j = json::parse(response);
                    newData.gridPower = j.value("site", json::object()).value("instant_power", 0.0);
                    newData.solarPower = j.value("solar", json::object()).value("instant_power", 0.0);
                    newData.homePower = j.value("load", json::object()).value("instant_power", 0.0);
                    newData.batteryPower = j.value("battery", json::object()).value("instant_power", 0.0);
                    newData.valid = true;
                } catch (...) {
                    newData.valid = false;
                }
            }
            
            // Fetch SOE sequentially to avoid lock races
            net_.fetchAsync(soeUrl, [this, alive, newData](std::string response) mutable {
                if (!alive->load(std::memory_order_acquire)) return;
                
                if (newData.valid && !response.empty()) {
                    try {
                        auto j = json::parse(response);
                        newData.batteryLevel = j.value("percentage", 100.0);
                    } catch (...) {}
                }
                
                isFetching_ = false;
                if (store_) store_->update(newData);
            }, 5);
        }, 5);
    }
}
