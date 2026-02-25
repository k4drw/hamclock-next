#pragma once

#include "../core/WinlinkData.h"
#include "../network/NetworkManager.h"
#include <memory>

class WinlinkProvider {
public:
  WinlinkProvider(NetworkManager &net, std::shared_ptr<WinlinkStore> store);

  // Fetch nearest Winlink RMS gateways.
  // radius: search radius in km (default 200).
  // Cache TTL = 1 hour (3600 s).
  void fetch(double lat, double lon, int radiusKm = 200, bool force = false);

private:
  static double haversineKm(double lat1, double lon1, double lat2, double lon2);

  NetworkManager &net_;
  std::shared_ptr<WinlinkStore> store_;
};
