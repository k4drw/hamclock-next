#pragma once

#include "../core/ForecastData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class ForecastProvider {
public:
  ForecastProvider(NetworkManager &net, std::shared_ptr<ForecastStore> store);

  // Fetch 7-day NWS forecast for the given location.
  // Requires two async round-trips: /points → /forecast.
  // Cache TTL is 1 hour (3600 s).
  void fetch(double lat, double lon, bool force = false);

private:
  void fetchForecast(const std::string &forecastUrl, bool force);

  NetworkManager &net_;
  std::shared_ptr<ForecastStore> store_;
};
