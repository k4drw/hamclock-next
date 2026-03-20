#pragma once

#include "ProviderBase.h"
#include "../core/DRAPData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <memory>
#include <string>

class DRAPProvider : public ProviderBase {
public:
  using DataCb = std::function<void(const std::string &data)>;

  DRAPProvider(NetworkManager &net,
               std::shared_ptr<DRAPDataStore> gridStore = nullptr);

  void fetch(DataCb cb);

private:
  NetworkManager &net_;
  std::shared_ptr<DRAPDataStore> gridStore_;
};
