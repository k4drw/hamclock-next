#pragma once

#include <ctime>
#include <filesystem>
#include <functional>
#include <mutex>
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

private:
  struct CacheEntry {
    std::string data;
    std::time_t timestamp;
    std::string lastModified;
    std::string etag;
  };
  std::unordered_map<std::string, CacheEntry> cache_;
  std::mutex cacheMutex_;
  std::filesystem::path cacheDir_;

  // Helper to compute safe filename for a URL (e.g. simple hash)
  std::string hashUrl(const std::string &url);
  void loadCache();
  void saveToDisk(const std::string &url, const CacheEntry &entry,
                  const std::string &data = "");
};
