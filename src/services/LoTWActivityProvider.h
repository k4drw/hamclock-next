#pragma once

#include "ProviderBase.h"
#include "../core/LoTWActivityData.h"
#include "../network/NetworkManager.h"
#include <memory>

// Fetches LoTW user activity data from public CSV and updates activity store.
// Source: https://lotw.arrl.org/lotw-user-activity.csv (updated daily)
// Interval: 24 hours (efficient reuse of once-daily CSV)
class LoTWActivityProvider : public ProviderBase {
public:
  LoTWActivityProvider(NetworkManager &net,
                       std::shared_ptr<LoTWActivityStore> store);

  void fetch();

private:
  NetworkManager &net_;
  std::shared_ptr<LoTWActivityStore> store_;

  static constexpr const char *LOTW_ACTIVITY_URL =
      "https://lotw.arrl.org/lotw-user-activity.csv";
};
