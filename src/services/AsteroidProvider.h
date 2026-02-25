#pragma once

#include "../core/AsteroidData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <map>
#include <mutex>
#include <string>

class AsteroidProvider {
public:
  using Callback = std::function<void(const AsteroidData &)>;

  explicit AsteroidProvider(NetworkManager &netMgr);

  // Updates the data if cache is stale (older than 24h)
  void update(bool force = false);

  AsteroidData getLatest() const;

  // Kick off async SBDB fetch for des; no-op if already cached.
  void fetchOrbitalElements(const std::string &des);

  // Returns cached elements or invalid struct if not yet fetched.
  OrbitalElements getOrbitalElements(const std::string &des) const;

private:
  void fetchInternal();
  void processResponse(const std::string &body);
  void processElementsResponse(const std::string &des, const std::string &body);
  std::string getCurrentDate() const;
  std::string getNextWeekDate() const;
  static std::string urlEncode(const std::string &s);

  NetworkManager &netMgr_;
  AsteroidData cachedData_;
  std::map<std::string, OrbitalElements> orbitalElementsCache_;
  mutable std::mutex mutex_;
  std::chrono::system_clock::time_point lastUpdate_;
  bool isFetching_ = false;
};
