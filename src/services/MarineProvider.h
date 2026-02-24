#pragma once

#include "../core/MarineData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class MarineProvider {
public:
  MarineProvider(NetworkManager &net, std::shared_ptr<MarineStore> store);

  // Fetch tides for the configured station and buoy observations.
  // tideStation: NOAA station ID (e.g. "8722670" for Lake Worth, FL).
  // buoyStation: NDBC buoy ID (e.g. "41114").
  // Cache TTL = 1 hour (3600 s) for tides, 30 min (1800 s) for buoy.
  void fetch(const std::string &tideStation, const std::string &buoyStation,
             bool force = false);

private:
  void fetchBuoy(const std::string &buoyStation, bool force);

  NetworkManager &net_;
  std::shared_ptr<MarineStore> store_;
};
