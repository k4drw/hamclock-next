#pragma once

#include "../network/NetworkManager.h"
#include <functional>
#include <string>

class SDOProvider {
public:
  using DataCb = std::function<void(const std::string &data)>;

  SDOProvider(NetworkManager &net);

  // Fetch latest SDO image (wavelength 0193, 304, etc)
  void fetch(const std::string &wavelength, DataCb cb);

private:
  NetworkManager &net_;
};
