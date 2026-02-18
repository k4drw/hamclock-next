#pragma once

#include "../network/NetworkManager.h"
#include <mutex>
#include <string>

class MufRtProvider {
public:
  MufRtProvider(NetworkManager &netMgr);

  void update();

  bool hasData() const;
  const std::string &getData() const;
  uint32_t getLastUpdateMs() const { return lastUpdateMs_; }

private:
  NetworkManager &netMgr_;
  std::string pngData_;
  bool hasData_ = false;
  uint32_t lastUpdateMs_ = 0;
  mutable std::mutex mutex_;
};
