#pragma once

#include "../core/AsteroidData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <mutex>
#include <string>

class AsteroidProvider {
public:
  using Callback = std::function<void(const AsteroidData &)>;

  explicit AsteroidProvider(NetworkManager &netMgr);

  // Updates the data if cache is stale (older than 24h)
  void update(bool force = false);

  AsteroidData getLatest() const;

private:
  void fetchInternal();
  void processResponse(const std::string &body);
  std::string getCurrentDate() const;
  std::string getNextWeekDate() const;

  NetworkManager &netMgr_;
  AsteroidData cachedData_;
  mutable std::mutex mutex_;
  std::chrono::system_clock::time_point lastUpdate_;
  bool isFetching_ = false;
};
