#pragma once

#include "ProviderBase.h"
#include "../core/ADIFData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class LoTWProvider : public ProviderBase {
public:
  LoTWProvider(NetworkManager &net, std::shared_ptr<ADIFStore> store,
               const std::string &call, const std::string &password);

  void fetch();
  void setCredentials(const std::string &call, const std::string &password);

private:
  NetworkManager &net_;
  std::shared_ptr<ADIFStore> store_;
  std::string call_;
  std::string password_;
  std::string lastSyncDate_;

  static constexpr const char *LOTW_API_URL =
      "https://lotw.arrl.org/lotwuser/lotwreport.adi";
};
