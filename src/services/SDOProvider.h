#pragma once

#include "ProviderBase.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <string>
#include <ctime>

#include <memory>

class SDOProvider : public ProviderBase, public std::enable_shared_from_this<SDOProvider> {
public:
  using DataCb = std::function<void(const std::string &data, std::time_t serverTime)>;

  SDOProvider(NetworkManager &net);

  // Fetch latest SDO image (wavelength 0193, 304, etc)
  void fetch(const std::string &wavelength, bool pfss, DataCb cb);

private:
  NetworkManager &net_;
};
