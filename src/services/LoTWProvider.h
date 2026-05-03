#pragma once

#include "ProviderBase.h"
#include "../core/ADIFData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <memory>
#include <string>

class LoTWProvider : public ProviderBase,
                     public std::enable_shared_from_this<LoTWProvider> {
public:
  using SyncStatusCallback = std::function<void(time_t, int, const std::string&)>;

  LoTWProvider(NetworkManager &net, std::shared_ptr<ADIFStore> store,
               const std::string &call, const std::string &password);

  void fetch();
  void setCredentials(const std::string &call, const std::string &password);
  void setStatusCallback(SyncStatusCallback cb) { statusCallback_ = cb; }

private:
  NetworkManager &net_;
  std::shared_ptr<ADIFStore> store_;
  std::string call_;
  std::string password_;
  std::string lastSyncDate_;
  SyncStatusCallback statusCallback_;

  static constexpr const char *LOTW_API_URL =
      "https://lotw.arrl.org/lotwuser/lotwreport.adi";
};
