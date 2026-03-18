#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <string>

class AuroraProvider : public ProviderBase {
public:
  using DataCb = std::function<void(const std::string &data)>;

  AuroraProvider(NetworkManager &net);

  // Fetch aurora forecast (North or South)
  void fetch(bool north, DataCb cb);

private:
  NetworkManager &net_;
};
