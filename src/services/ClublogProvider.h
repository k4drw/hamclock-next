#pragma once

#include "ProviderBase.h"
#include "../core/ClublogData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class ClublogProvider : public ProviderBase {
public:
  ClublogProvider(NetworkManager &net, std::shared_ptr<ClublogStore> store,
                  const std::string &apiKey);

  void fetch();
  void setApiKey(const std::string &key) { apiKey_ = key; }

private:
  NetworkManager &net_;
  std::shared_ptr<ClublogStore> store_;
  std::string apiKey_;

  static constexpr const char *CLUBLOG_API_BASE =
      "https://clublog.org/mostwanted.php";
};
