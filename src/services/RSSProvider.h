#pragma once

#include "../core/RSSData.h"
#include "../network/NetworkManager.h"

#include <memory>

class RSSProvider {
public:
    RSSProvider(NetworkManager& net, std::shared_ptr<RSSDataStore> store);

    void fetch();

    // When disabled, fetch() is a no-op (stops network activity).
    void setEnabled(bool enabled) { enabled_ = enabled; }

private:
    NetworkManager& net_;
    std::shared_ptr<RSSDataStore> store_;
    bool enabled_ = true;
};
