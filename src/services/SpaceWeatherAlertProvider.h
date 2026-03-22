#pragma once

#include "ProviderBase.h"
#include "../core/SpaceWeatherAlertData.h"
#include "../network/NetworkManager.h"
#include <memory>

class SpaceWeatherAlertProvider : public ProviderBase {
public:
  SpaceWeatherAlertProvider(NetworkManager &net,
                             std::shared_ptr<SpaceWeatherAlertStore> store);

  // Fetch recent NOAA SWPC alerts. NetworkManager caches for 15 minutes.
  void fetch();

private:
  NetworkManager &net_;
  std::shared_ptr<SpaceWeatherAlertStore> store_;

  static constexpr const char *ALERTS_URL =
      "https://services.swpc.noaa.gov/products/alerts.json";
};
