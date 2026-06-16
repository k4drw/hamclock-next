#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <ctime>

struct LaunchEvent {
    std::string id;
    std::string missionName;
    std::string rocketName;
    std::string providerName;
    std::string statusName;
    std::time_t windowStart;
    double padLat;
    double padLon;
    std::string padName;
    bool hasLocation;
};

class LaunchProvider : public ProviderBase, public std::enable_shared_from_this<LaunchProvider> {
public:
    explicit LaunchProvider(NetworkManager& net);
    ~LaunchProvider() override = default;

    void fetch(bool force = false);
    std::vector<LaunchEvent> getUpcoming() const;

private:
    void parse(const std::string& raw);
    
    NetworkManager& net_;
    std::vector<LaunchEvent> upcomingLaunches_;
    mutable std::mutex mutex_;
    uint32_t lastFetch_;
    bool dataValid_ = false;
    
    static constexpr const char* API_URL = "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=10";
};
