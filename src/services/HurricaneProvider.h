#pragma once

#include "../core/HurricaneData.h"
#include "../network/NetworkManager.h"
#include <memory>

class HurricaneProvider {
public:
  HurricaneProvider(NetworkManager &net,
                    std::shared_ptr<HurricaneStore> store);

  // Fetch active storm list from NHC. Cache TTL = 30 min (1800 s).
  void fetch(bool force = false);

private:
  NetworkManager &net_;
  std::shared_ptr<HurricaneStore> store_;
};
