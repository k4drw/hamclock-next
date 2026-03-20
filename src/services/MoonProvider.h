#pragma once

#include "../core/MoonData.h"
#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <memory>

class MoonProvider : public ProviderBase {
public:
  MoonProvider(NetworkManager &net, std::shared_ptr<MoonStore> store);

  void update(double lat, double lon);

private:
  NetworkManager &net_;
  std::shared_ptr<MoonStore> store_;
};
