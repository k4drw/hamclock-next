#pragma once

#include "../network/NetworkManager.h"
#include <functional>
#include <string>

class DRAPProvider {
public:
  using DataCb = std::function<void(const std::string &data)>;

  DRAPProvider(NetworkManager &net);

  void fetch(DataCb cb);

private:
  NetworkManager &net_;
};
