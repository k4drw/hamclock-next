#pragma once

#include "../core/ConfigManager.h"

#include <ctime>
#include <filesystem>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>

class NetworkManager {
public:
  explicit NetworkManager(const std::filesystem::path &cacheDir = "");
  ~NetworkManager() = default;

  NetworkManager(const NetworkManager &) = delete;
  NetworkManager &operator=(const NetworkManager &) = delete;

  // Fetches URL content asynchronously.
  // If 'force' is false, it may return a cached response if within
  // 'cacheAgeSeconds'. Default cache age is 60 minutes (3600 seconds) to avoid
  // rate limits.
  void fetchAsync(const std::string &url,
                  std::function<void(std::string)> callback,
                  int cacheAgeSeconds = 3600, bool force = false);

  // Set CORS proxy prefix (WASM only). Called at startup from AppConfig.
  // All subsequent fetchAsync calls prepend this to external URLs.
  void setCorsProxyUrl(const std::string &url) { corsProxyUrl_ = url; }

  // Configure Local Data Hub mode. Called at startup and on config reload.
  void setHubConfig(HubMode mode, const std::string &ip, int port);

private:
  struct CacheEntry {
    std::string data;
    std::time_t timestamp;
    std::time_t serverTime = 0;
    std::string lastModified;
    std::string etag;
  };
  std::unordered_map<std::string, CacheEntry> cache_;
  std::mutex cacheMutex_;
  std::filesystem::path cacheDir_;
  std::string corsProxyUrl_;

  HubMode     hubMode_  = HubMode::Off;
  std::string hubIp_;
  int         hubPort_  = 8080;
  std::mutex  hubMutex_;

  std::set<std::string> activeFetches_;
  std::mutex fetchMutex_;

  // Helper to compute safe filename for a URL (e.g. simple hash)
  std::string hashUrl(const std::string &url);
  void loadCache();
  void saveToDisk(const std::string &url, const CacheEntry &entry,
                  const std::string &data = "");
  std::string fetchFromHubSync(const std::string &hubUrl);
  void fetchDirect(const std::string &url,
                   std::function<void(std::string)> callback,
                   bool hasCache, const CacheEntry &cached);

public:
  // Get the server-reported last modified time for a cached URL
  std::time_t getCacheServerTime(const std::string &url);
};
