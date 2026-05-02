#pragma once

#include "ProviderBase.h"
#include "../core/ForecastData.h"
#include "../network/NetworkManager.h"
#include <memory>

class OpenMeteoForecastProvider
    : public ProviderBase,
      public std::enable_shared_from_this<OpenMeteoForecastProvider> {
public:
  OpenMeteoForecastProvider(NetworkManager &net,
                            std::shared_ptr<ForecastStore> store);

  void fetch(double lat, double lon, bool force = false);

private:
  NetworkManager &net_;
  std::shared_ptr<ForecastStore> store_;
};
