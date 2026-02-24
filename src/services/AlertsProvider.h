#pragma once

#include "../core/AlertsData.h"
#include "../network/NetworkManager.h"
#include <memory>

class AlertsProvider {
public:
  AlertsProvider(NetworkManager &net, std::shared_ptr<AlertsStore> store);

  // Fetch active NWS alerts for the given location.
  // Cache TTL is 5 minutes (300 s).
  void fetch(double lat, double lon, bool force = false);

private:
  NetworkManager &net_;
  std::shared_ptr<AlertsStore> store_;
};
