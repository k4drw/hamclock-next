#pragma once

#include "ProviderBase.h"
#include "../core/ActivityData.h"
#include "../core/PrefixManager.h"
#include "../network/NetworkManager.h"
#include <memory>

class ActivityProvider : public ProviderBase {
public:
  enum class UpdateType { DXPeds, POTA, SOTA };

  ActivityProvider(NetworkManager &net,
                   std::shared_ptr<ActivityDataStore> store,
                   PrefixManager &prefixMgr);

  void fetch();

private:
  void fetchDXPeds();
  void fetchPOTA();
  void fetchSOTA();

  NetworkManager &net_;
  std::shared_ptr<ActivityDataStore> store_;
  PrefixManager &prefixMgr_;

  static constexpr const char *DX_PEDS_URL =
      "https://www.ng3k.com/Misc/adxo.html";
  static constexpr const char *POTA_API_URL = "https://api.pota.app/spot";
  static constexpr const char *SOTA_API_URL =
      "https://api2.sota.org.uk/api/spots/50";
};
