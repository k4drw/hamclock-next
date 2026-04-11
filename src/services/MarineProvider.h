#pragma once

#include "ProviderBase.h"
#include "../core/MarineData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

struct HamClockState;

class MarineProvider : public ProviderBase {
public:
  MarineProvider(NetworkManager &net, std::shared_ptr<MarineStore> store,
                 struct HamClockState *state);

  // Fetch tides for the configured station and buoy observations.
  // tideStation: NOAA station ID (e.g. "8722670" for Lake Worth, FL).
  // buoyStation: NDBC buoy ID (e.g. "41114").
  // Cache TTL = 1 hour (3600 s) for tides, 30 min (1800 s) for buoy.
  void fetch(const std::string &tideStation, const std::string &buoyStation,
             bool force = false);

  // Search for the closest NOAA tide station and NDBC buoy to the given lat/lon.
  // Results are delivered via AE_MARINE_LOOKUP_READY event.
  void lookupClosestStations(double lat, double lon);

private:

  void fetchBuoy(const std::string &buoyStation, bool force);
  void fetchStationName(const std::string &tideStation);


  NetworkManager &net_;
  std::shared_ptr<MarineStore> store_;
  struct HamClockState *state_;
};
