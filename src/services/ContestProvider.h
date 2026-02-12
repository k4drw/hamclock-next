#pragma once

#include "../core/ContestData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class ContestProvider {
public:
  ContestProvider(NetworkManager &net, std::shared_ptr<ContestStore> store);

  void fetch();

private:
  void processData(const std::string &body);

  NetworkManager &net_;
  std::shared_ptr<ContestStore> store_;

  static constexpr const char *CONTEST_URL =
      "https://www.contestcalendar.com/calendar.rss";
};
