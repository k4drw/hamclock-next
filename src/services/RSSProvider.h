#pragma once

#include "../core/RSSData.h"
#include "../network/NetworkManager.h"

#include <memory>

class RSSProvider {
public:
    RSSProvider(NetworkManager& net, std::shared_ptr<RSSDataStore> store);

    void fetch();

private:
    NetworkManager& net_;
    std::shared_ptr<RSSDataStore> store_;
};
