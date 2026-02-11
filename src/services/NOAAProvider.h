#pragma once

#include "../core/SolarData.h"
#include "../network/NetworkManager.h"

#include <memory>

class NOAAProvider {
public:
    NOAAProvider(NetworkManager& net, std::shared_ptr<SolarDataStore> store);

    void fetch();

private:
    static constexpr const char* K_INDEX_URL =
        "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json";

    NetworkManager& net_;
    std::shared_ptr<SolarDataStore> store_;
};
