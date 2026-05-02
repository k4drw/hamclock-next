#pragma once

#include "../core/ConfigManager.h"

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class NetworkManager {
public:
  explicit NetworkManager(const std::filesystem::path &cacheDir = "");
  // Signal shutdown and block until every detached fetch thread that already
  // passed the alive_ check has finished touching our members.  This closes
  // the TOCTOU window: the alive_ write and the inflight_ check are both done
  // under inflightMutex_, so a thread cannot slip in between.
  ~NetworkManager() {
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      alive_->store(false, std::memory_order_release);
    }
    std::unique_lock<std::mutex> lk(inflightMutex_);
    inflightCv_.wait(lk, [this] { return inflight_ == 0; });
  }

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

  // Block until the background loadCache() thread has finished populating the
  // in-memory cache index.  Call this once, just before DashboardContext is
  // created, so provider fetchAsync() calls always find their cache entries.
  // Returns immediately if loadCache() already completed (the common case once
  // SDL init has consumed most of the time budget).
  void waitForCacheLoad();

  // Shared string type to avoid large allocations when multiple clients request
  // the same URL concurrently from a Hub Master.
  using SharedString = std::shared_ptr<const std::string>;
  using SharedCallback = std::function<void(SharedString)>;

  void fetchSharedAsync(const std::string &url, SharedCallback callback,
                        int cacheAgeSeconds = 3600, bool force = false);

private:
  struct CacheEntry {
    SharedString data;
    std::time_t timestamp;
    std::time_t serverTime = 0;
    std::string lastModified;
    std::string etag;
  };
  std::unordered_map<std::string, CacheEntry> cache_;
  std::mutex cacheMutex_;
  std::filesystem::path cacheDir_;
  std::string corsProxyUrl_;

  HubMode     hubIpMode_  = HubMode::Off; // Keep for binary compatibility if needed
  HubMode     hubMode_  = HubMode::Off;
  std::string hubIp_;
  int         hubPort_  = 8080;
  std::mutex  hubMutex_;

  // Maps in-flight URL → list of callbacks waiting on the same fetch.
  // When a fetch completes, the primary callback plus all queued callbacks are called.
  std::unordered_map<std::string, std::vector<SharedCallback>> activeFetches_;
  std::mutex fetchMutex_;

  // Shared with all detached fetch threads. alive_ is set to false and the
  // destructor waits for all in-flight threads to finish before returning,
  // preventing use-after-free on destroyed members.
  std::shared_ptr<std::atomic<bool>> alive_ =
      std::make_shared<std::atomic<bool>>(true);
  std::mutex inflightMutex_;
  std::condition_variable inflightCv_;
  int inflight_ = 0;

  // Tracks whether the background loadCache() thread has finished.
  // Separate from inflight_ so waitForCacheLoad() has a dedicated signal.
  bool cacheLoadDone_ = true; // true means either not started or complete
  std::mutex cacheLoadMutex_;
  std::condition_variable cacheLoadCv_;

  // Helper to compute safe filename for a URL (e.g. simple hash)
  std::string hashUrl(const std::string &url);
  void loadCache();
  void saveToDisk(const std::string &url, const CacheEntry &entry,
                  const std::string &data = "");
  std::string fetchFromHubSync(const std::string &hubUrl, long &httpCode);
  void fetchDirect(const std::string &url, SharedCallback callback,
                   bool hasCache, const CacheEntry &cached);
  SharedString loadFromDiskCache(const std::string &url);

  // LRU and RAM cache management
  void updateLruAndPrune(const std::string &url, size_t dataSize);
  void removeFromLru(const std::string &url);
  std::list<std::string> lru_;
  size_t totalRamBytes_ = 0;
  const size_t MAX_RAM_BYTES = 50 * 1024 * 1024; // 50 MB RAM cap

#ifdef __ANDROID__
  // Concatenates Android's per-file system CA store into a single PEM bundle
  // that mbedTLS can use via CURLOPT_CAINFO. Called once from the constructor.
  void buildAndroidCaBundle();
  std::string androidCaBundlePath_;
#endif

public:
  // Get the server-reported last modified time for a cached URL
  std::time_t getCacheServerTime(const std::string &url);

  // Get metadata for conditional GET
  bool getCacheMetadata(const std::string &url, std::string &lastModified,
                        std::string &etag, std::time_t &timestamp);

  // Periodic cleanup of entries older than maxAgeSeconds
  void pruneStaleCache(int maxAgeSeconds = 86400);

  // Cache stats for debugging
  size_t getCacheRamBytes() const { return totalRamBytes_; }
  size_t getCacheItemCount() const { return cache_.size(); }

  // Get the local IPv4 address (excluding loopback).
  static std::string getLocalIP();
};
