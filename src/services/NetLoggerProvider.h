#pragma once

#include "../core/NetLoggerData.h"
#include <memory>
#include <atomic>
#include <chrono>
#include <string>

class NetworkManager;

class NetLoggerProvider {
public:
    explicit NetLoggerProvider(NetworkManager& net);
    ~NetLoggerProvider();
    
    void fetch();
    
    // Test helper or fallback
    const NetLoggerData& getLatestData() const { return data_; }

private:
    NetworkManager& net_;
    
    NetLoggerData data_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    
    std::chrono::steady_clock::time_point lastNetsFetchTime_;
    std::chrono::steady_clock::time_point lastCheckinsFetchTime_;
    
    static constexpr int NETS_FETCH_INTERVAL_S = 60; // 1 per minute
    static constexpr int CHECKINS_FETCH_INTERVAL_S = 20; // 3 per minute
    
    bool isFetchingNets_ = false;
    bool isFetchingCheckins_ = false;
    
    std::string urlEncode(const std::string& value);
    void parseNets(const std::string& xml);
    void parseCheckins(const std::string& xml);
    
    void gridToLatLon(const std::string& grid, double& lat, double& lon, bool& hasLoc);
    std::string extractTag(const std::string& xml, const std::string& tag, size_t startPos, size_t& endPos);
};
