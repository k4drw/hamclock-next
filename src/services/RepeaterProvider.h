#pragma once

#include "../core/RepeaterData.h"
#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class RepeaterProvider : public ProviderBase {
public:
  RepeaterProvider(NetworkManager &net, std::shared_ptr<RepeaterStore> store);

  // Fetch repeaters near the given lat/lon from RepeaterBook.
  // state: 2-letter US state code (e.g. "FL"). Empty = nationwide.
  // Cache TTL is 24 hours (86400 s).
  void fetch(double lat, double lon, const std::string &state = "",
             bool force = false);

private:
  // Haversine distance in km
  static double haversineKm(double lat1, double lon1, double lat2, double lon2);

  NetworkManager &net_;
  std::shared_ptr<RepeaterStore> store_;
};
