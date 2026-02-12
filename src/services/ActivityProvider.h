#pragma once

#include "../core/ActivityData.h"
#include "../network/NetworkManager.h"
#include <memory>

class ActivityProvider {
public:
  ActivityProvider(NetworkManager &net,
                   std::shared_ptr<ActivityDataStore> store);

  void fetch();

private:
  void fetchDXPeds();
  void fetchPOTA();
  void fetchSOTA();

  NetworkManager &net_;
  std::shared_ptr<ActivityDataStore> store_;

  static constexpr const char *DX_PEDS_URL =
      "https://www.ng3k.com/Misc/adxo.html";
  static constexpr const char *POTA_API_URL = "https://api.pota.app/spot/info";
  static constexpr const char *SOTA_API_URL =
      "https://api2.sota.org.uk/api/spots";
};
