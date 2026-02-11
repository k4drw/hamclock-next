#pragma once

#include "../core/LiveSpotData.h"
#include "../network/NetworkManager.h"

#include <memory>
#include <string>

class LiveSpotProvider {
public:
    LiveSpotProvider(NetworkManager& net,
                     std::shared_ptr<LiveSpotDataStore> store,
                     const std::string& callsign,
                     const std::string& grid);

    void fetch();

private:
    NetworkManager& net_;
    std::shared_ptr<LiveSpotDataStore> store_;
    std::string callsign_;
    std::string grid_;
};
