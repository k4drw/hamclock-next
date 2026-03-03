#pragma once

#include "../network/NetworkManager.h"
#include <functional>
#include <string>
#include <ctime>

class SDOProvider {
public:
  using DataCb = std::function<void(const std::string &data, std::time_t serverTime)>;

  SDOProvider(NetworkManager &net);

  // Fetch latest SDO image (wavelength 0193, 304, etc)
  void fetch(const std::string &wavelength, bool pfss, DataCb cb);

private:
  NetworkManager &net_;
};
