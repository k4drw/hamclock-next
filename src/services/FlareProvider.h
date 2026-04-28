#pragma once

#include "../core/FlareData.h"
#include "ProviderBase.h"
#include <memory>
#include <string>

class NetworkManager;

class FlareProvider : public ProviderBase,
                      public std::enable_shared_from_this<FlareProvider> {
public:
  FlareProvider(NetworkManager &net, std::shared_ptr<FlareDataStore> store);
  ~FlareProvider();

  void fetch();

private:
  NetworkManager &net_;
  std::shared_ptr<FlareDataStore> store_;

  FlareData parseFlareJSON(const std::string &json);
};
