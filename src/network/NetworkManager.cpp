#include "NetworkManager.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#endif

#include <thread>

#include <filesystem>
#include <fstream>

#include <cctype> // For std::tolower
#include <sstream>
#include <unordered_map> // For header parsing
#ifdef __ANDROID__
#include <dirent.h>
#endif

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#endif

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

static size_t writeCallback(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  auto *response = static_cast<std::string *>(userdata);
  response->append(ptr, size * nmemb);
  return size * nmemb;
}

static size_t headerCallback(char *ptr, size_t size, size_t nmemb,
                             void *userdata) {
  auto *headers =
      static_cast<std::unordered_map<std::string, std::string> *>(userdata);
  std::string line(ptr, size * nmemb);
  auto colon = line.find(':');
  if (colon != std::string::npos) {
    std::string key = line.substr(0, colon);
    std::string value = line.substr(colon + 1);

    // Trim whitespace safely
    auto first = value.find_first_not_of(" \t\r\n");
    if (first != std::string::npos) {
      auto last = value.find_last_not_of(" \t\r\n");
      value = value.substr(first, last - first + 1);
    } else {
      value.clear();
    }

    // Lowercase key for consistent lookup
    for (auto &c : key)
      c = std::tolower(c);
    (*headers)[key] = value;
  }
  return size * nmemb;
}

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

// ... (existing includes)

// URL-safe base64 (RFC 4648 §5): uses '-' and '_' instead of '+' and '/'
// so the encoded string can be placed in URL query parameters without
// percent-encoding.  Padding is omitted (the decoder handles this).
static const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static std::string base64Encode(const std::string &in) {
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(kB64Chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(kB64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
  // No '=' padding — decoder handles unpadded input
  return out;
}

void NetworkManager::setHubConfig(HubMode mode, const std::string &ip,
                                  int port) {
  std::lock_guard<std::mutex> lk(hubMutex_);
  hubMode_ = mode;
  hubIp_ = ip;
  hubPort_ = port;
}

std::string NetworkManager::fetchFromHubSync(const std::string &hubUrl,
                                            long &httpCode) {
#ifdef __EMSCRIPTEN__
  httpCode = 0;
  return "";
#else
  std::string result;
  CURL *curl = curl_easy_init();
  if (!curl) {
    httpCode = 0;
    return "";
  }
  curl_easy_setopt(curl, CURLOPT_URL, hubUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
  CURLcode rc = curl_easy_perform(curl);
  httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK)
    return "";
  return result;
#endif
}

// Basic in-memory cache to prevent accidental tight-loop fetches
void NetworkManager::fetchAsync(const std::string &url,
                                std::function<void(std::string)> callback,
                                int cacheAgeSeconds, bool force) {
  fetchSharedAsync(
      url,
      [callback](SharedString data) {
        if (data)
          callback(*data);
        else
          callback("");
      },
      cacheAgeSeconds, force);
}

void NetworkManager::fetchSharedAsync(const std::string &url,
                                      SharedCallback callback,
                                      int cacheAgeSeconds, bool force) {
  // Check memory cache first
  CacheEntry cached;
  bool hasCache = false;

  {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cache_.find(url);
    if (it != cache_.end()) {
      cached = it->second;
      hasCache = true;
    }
  }

  if (hasCache && !force) {
    std::time_t now = std::time(nullptr);
    if (now - cached.timestamp < cacheAgeSeconds) {
      if (cached.data && !cached.data->empty()) {
        LOG_T("NetworkManager", "Memory cache hit for {}", url);
        {
          std::lock_guard<std::mutex> lock(cacheMutex_);
          updateLruAndPrune(url, cached.data->size());
        }
        callback(cached.data);
        return;
      } else {
        // Deduplicate in-flight disk loads
        {
          std::lock_guard<std::mutex> lock(fetchMutex_);
          if (activeFetches_.count(url)) {
            LOG_T("NetworkManager",
                  "Disk load already in progress for {}, queuing callback",
                  url);
            activeFetches_[url].push_back(std::move(callback));
            return;
          }
          activeFetches_[url] = {};
        }

        LOG_T("NetworkManager",
              "Memory record found but no data (too large), loading from disk "
              "for {}",
              url);
        WorkerService::getInstance().submitTask([this, url, callback = std::move(callback),
                     alive = alive_]() {
          {
            std::lock_guard<std::mutex> lk(inflightMutex_);
            if (!alive->load(std::memory_order_relaxed))
              return;
            ++inflight_;
          }
          SharedString data = loadFromDiskCache(url);
          std::vector<SharedCallback> pending;
          {
            std::lock_guard<std::mutex> lock(fetchMutex_);
            pending = std::move(activeFetches_[url]);
            activeFetches_.erase(url);
          }
          callback(data);
          for (auto &cb : pending)
            cb(data);
          {
            std::lock_guard<std::mutex> lk(inflightMutex_);
            if (--inflight_ == 0)
              inflightCv_.notify_all();
          }
        });
        return;
      }
    } else {
      // Cache entry is stale — clear payload but keep metadata for
      // If-Modified-Since
      std::lock_guard<std::mutex> lock(cacheMutex_);
      removeFromLru(url);
      auto it = cache_.find(url);
      if (it != cache_.end() && it->second.data) {
        it->second.data.reset();
      }
    }
  }

  // Deduplicate in-flight network fetches
  {
    std::lock_guard<std::mutex> lock(fetchMutex_);
    if (activeFetches_.count(url)) {
      LOG_T("NetworkManager",
            "Network fetch already in progress for {}, queuing callback", url);
      activeFetches_[url].push_back(std::move(callback));
      return;
    }
    activeFetches_[url] = {};
  }

  auto safeCallback = [this, url, callback = std::move(callback)](SharedString data) {
    std::vector<SharedCallback> pending;
    {
      std::lock_guard<std::mutex> lock(fetchMutex_);
      pending = std::move(activeFetches_[url]);
      activeFetches_.erase(url);
    }
    callback(data);
    for (auto &cb : pending)
      cb(data);
  };

#ifdef __EMSCRIPTEN__
  std::string fetchUrl = url;
  if (!corsProxyUrl_.empty() && url.find("http") == 0) {
    fetchUrl = corsProxyUrl_ + url;
  }

  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  std::strncpy(attr.requestMethod, "GET", sizeof(attr.requestMethod) - 1);
  attr.requestMethod[sizeof(attr.requestMethod) - 1] = '\0';
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  // Capture callback in a heap-allocated wrapper. Ownership is handed to
  // emscripten via attr.userData and reclaimed as a unique_ptr at the top of
  // whichever callback fires, so an early return (or an added throw site)
  // still frees it.
  struct FetchCtx {
    NetworkManager *mgr;
    std::string url;
    SharedCallback cb;
  };
  auto ctx = std::make_unique<FetchCtx>(
      FetchCtx{this, url, std::move(safeCallback)});

  attr.onsuccess = [](emscripten_fetch_t *fetch) {
    std::unique_ptr<FetchCtx> owned(static_cast<FetchCtx *>(fetch->userData));
    if (fetch->data && fetch->numBytes > 0) {
      std::string response(fetch->data, fetch->numBytes);

      // Update in-memory cache
      {
        std::lock_guard<std::mutex> lock(owned->mgr->cacheMutex_);
        CacheEntry entry;
        entry.timestamp = std::time(nullptr);
        // We don't have header headers easily in simple fetch on WASM,
        // but we can at least cache the data for small responses.
        if (response.size() < 512 * 1024) {
          entry.data = std::make_shared<const std::string>(response);
        }
        owned->mgr->cache_[owned->url] = entry;
      }

      owned->cb(std::make_shared<const std::string>(std::move(response)));
    } else {
      owned->cb(nullptr);
    }
    emscripten_fetch_close(fetch);
  };
  attr.onerror = [](emscripten_fetch_t *fetch) {
    std::unique_ptr<FetchCtx> owned(static_cast<FetchCtx *>(fetch->userData));
    LOG_E("NetworkManager", "WASM fetch failed for {} (status {})", owned->url,
          fetch->status);
    owned->cb(nullptr); // null = failure
    emscripten_fetch_close(fetch);
  };

  attr.userData = ctx.release();
  emscripten_fetch(&attr, fetchUrl.c_str());
#else
  // --- Hub client proxy ---
  {
    std::lock_guard<std::mutex> lk(hubMutex_);
    if (hubMode_ == HubMode::Client && !hubIp_.empty()) {
      std::string encoded = base64Encode(url);
      std::string hubUrl = "http://" + hubIp_ + ":" + std::to_string(hubPort_) +
                           "/api/hub/fetch?url=" + encoded +
                           "&max_age=" + (force ? "0" : std::to_string(cacheAgeSeconds));
      if (hasCache) {
        if (!cached.lastModified.empty())
          hubUrl += "&ims=" + base64Encode(cached.lastModified);
        if (!cached.etag.empty())
          hubUrl += "&inm=" + base64Encode(cached.etag);
      }

      LOG_D("NetworkManager", "Hub client: Proxying request for {} to Hub at {}", url, hubUrl);

      WorkerService::getInstance().submitTask([this, hubUrl, url, callback = std::move(safeCallback), hasCache,
                   cached, alive = alive_]() mutable {
        {
          std::lock_guard<std::mutex> lk(inflightMutex_);
          if (!alive->load(std::memory_order_relaxed))
            return;
          ++inflight_;
        }
        long httpCode = 0;
        std::string body = fetchFromHubSync(hubUrl, httpCode);
        if (httpCode == 304) {
          LOG_D("NetworkManager", "Hub client: 304 Not Modified from Hub for {}",
                url);
          SharedString retData = cached.data;
          if (!retData) {
            retData = loadFromDiskCache(url);
          }
          {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[url].timestamp = std::time(nullptr);
          }
          callback(std::move(retData));
        } else if (!body.empty()) {
          LOG_D("NetworkManager",
                "Hub client: Received {} bytes from Hub for {}", body.size(),
                url);
          auto sharedBody = std::make_shared<const std::string>(std::move(body));
          {
            CacheEntry entry;
            entry.timestamp = std::time(nullptr);

            bool isLarge = sharedBody->size() > 512 * 1024;
            bool isImage = url.find(".jpg") != std::string::npos ||
                           url.find(".png") != std::string::npos ||
                           url.find("image") != std::string::npos;

            std::lock_guard<std::mutex> lock(cacheMutex_);
            if (hubMode_ == HubMode::Master || (!isLarge && !isImage)) {
              entry.data = sharedBody;
              updateLruAndPrune(url, sharedBody->size());
            } else {
              removeFromLru(url);
              lru_.push_front(url);
              while ((int)cache_.size() > (int)MAX_CACHE_ENTRIES && !lru_.empty()) {
                std::string oldest = lru_.back();
                auto it = cache_.find(oldest);
                if (it != cache_.end()) {
                  if (it->second.data) totalRamBytes_ -= it->second.data->size();
                  cache_.erase(it);
                }
                lru_.pop_back();
              }
            }

            cache_[url] = entry;
            if (!cacheDir_.empty())
              saveToDisk(url, entry, *sharedBody);
          }
          callback(std::move(sharedBody));
        } else {
          LOG_D("NetworkManager",
                "Hub client: Hub miss or error for {}, falling back to direct",
                url);
          fetchDirect(url, std::move(callback), hasCache, cached);
        }
        {
          std::lock_guard<std::mutex> lk(inflightMutex_);
          if (--inflight_ == 0)
            inflightCv_.notify_all();
        }
      });
      return;
    }
  }
  // --- Direct fetch ---
  WorkerService::getInstance().submitTask([this, url, callback = std::move(safeCallback), hasCache,
               cached, alive = alive_]() mutable {
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (!alive->load(std::memory_order_relaxed))
        return;
      ++inflight_;
    }
    fetchDirect(url, std::move(callback), hasCache, cached);
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (--inflight_ == 0)
        inflightCv_.notify_all();
    }
  });
#endif
}

std::time_t NetworkManager::getCacheServerTime(const std::string &url) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  auto it = cache_.find(url);
  if (it != cache_.end()) {
    return it->second.serverTime;
  }
  return 0;
}

NetworkManager::SharedString NetworkManager::loadFromDiskCache(const std::string &url) {
  if (cacheDir_.empty())
    return nullptr;
  std::filesystem::path p = cacheDir_ / hashUrl(url);
  std::ifstream ifs(p, std::ios::binary);
  if (ifs) {
    std::string line;
    if (std::getline(ifs, line)) {
      bool v11 = (line == "HamClockCache/1.1");
      int skip = v11 ? 5 : 4;
      for (int i = 0; i < skip; ++i)
        std::getline(ifs, line);
      return std::make_shared<const std::string>(
          std::istreambuf_iterator<char>(ifs),
          std::istreambuf_iterator<char>());
    }
  }
  return nullptr;
}

bool NetworkManager::getCacheMetadata(const std::string &url,
                                      std::string &lastModified,
                                      std::string &etag,
                                      std::time_t &timestamp) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  auto it = cache_.find(url);
  if (it != cache_.end()) {
    lastModified = it->second.lastModified;
    etag = it->second.etag;
    timestamp = it->second.timestamp;
    return true;
  }
  return false;
}

#ifndef __EMSCRIPTEN__
void NetworkManager::fetchDirect(const std::string &url, SharedCallback callback,
                                 bool hasCache, const CacheEntry &cached) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    LOG_E("NetworkManager", "curl_easy_init failed");
    callback(nullptr);
    return;
  }

  std::unordered_map<std::string, std::string> headers;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

  if (url.find("pskreporter.info") != std::string::npos) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamClock/1.0");
    curl_easy_setopt(curl,
                     CURLOPT_REFERER, "https://pskreporter.info/pskmap.html");
  } else {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamClock-Next/1.0");
  }

  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
  curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

  // Workaround for lmsal.com and pskreporter.info certificate issues
  if (url.find("lmsal.com") != std::string::npos ||
      url.find("pskreporter.info") != std::string::npos) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }

// On Linux with static mbedTLS, we often need to point CURL to the CA
// bundle. However, for system libcurl (dynamic), this is usually automatic.
// We remove the hardcoded path to let libcurl decide.
// Note: Android defines __linux__ but has no CA bundle at standard paths.
// Static mbedTLS cannot use Android's per-file /system/etc/security/cacerts/
// store, so we disable peer verification there. All fetched data is public
// (NOAA, ham radio databases); no sensitive credentials traverse these
// connections.
#if defined(__ANDROID__)
  // Use the PEM bundle built from Android's system CA store at startup.
  if (!androidCaBundlePath_.empty())
    curl_easy_setopt(curl, CURLOPT_CAINFO, androidCaBundlePath_.c_str());
#elif defined(__linux__)
  if (std::filesystem::exists("/etc/ssl/certs/ca-certificates.crt")) {
    curl_easy_setopt(curl, CURLOPT_CAINFO,
                     "/etc/ssl/certs/ca-certificates.crt");
  } else if (std::filesystem::exists("/etc/pki/tls/certs/ca-bundle.crt")) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/pki/tls/certs/ca-bundle.crt");
  } else if (std::filesystem::exists("/etc/ssl/ca-bundle.pem")) {
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/ca-bundle.pem");
  }
#endif

  // Use Conditional GET (If-Modified-Since / If-None-Match) to save bandwidth
  struct curl_slist *chunk = nullptr;
  if (hasCache) {
    if (!cached.etag.empty()) {
      std::string h = "If-None-Match: " + cached.etag;
      chunk = curl_slist_append(chunk, h.c_str());
    }
    if (!cached.lastModified.empty()) {
      std::string h = "If-Modified-Since: " + cached.lastModified;
      chunk = curl_slist_append(chunk, h.c_str());
    }
    if (chunk) {
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
  }

  auto response = std::make_shared<std::string>();
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, response.get());

  LOG_D("NetworkManager", "Fetching from network: {}", url);
  CURLcode res = curl_easy_perform(curl);

  if (chunk) {
    curl_slist_free_all(chunk);
  }

  long responseCode = 0;
  long fileTime = -1;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
  curl_easy_getinfo(curl, CURLINFO_FILETIME, &fileTime);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    LOG_E("NetworkManager", "Fetch failed for {}: {}", url,
          curl_easy_strerror(res));
    callback(nullptr);
    return;
  }

  // Handle 304 Not Modified
  if (responseCode == 304) {
    LOG_D("NetworkManager", "304 Not Modified for {}", url);
    SharedString retData = cached.data;
    if (!retData) {
      retData = loadFromDiskCache(url);
    }
    {
      std::lock_guard<std::mutex> lock(cacheMutex_);
      cache_[url].timestamp = std::time(nullptr);
    }
    callback(std::move(retData));
    return;
  }

  if (responseCode != 200) {
    LOG_E("NetworkManager", "HTTP error {} for {}", responseCode, url);
    callback(nullptr);
    return;
  }

  // Update cache on success (200 OK)
  {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    std::time_t now = std::time(nullptr);
    CacheEntry entry;
    entry.timestamp = now;
    entry.serverTime = (fileTime != -1) ? (std::time_t)fileTime : 0;
    if (headers.count("last-modified"))
      entry.lastModified = headers.at("last-modified");
    if (headers.count("etag"))
      entry.etag = headers.at("etag");

    bool isLarge = response->size() > 512 * 1024;
    bool isImage =
        url.find(".jpg") != std::string::npos ||
        url.find(".png") != std::string::npos ||
        url.find("image") != std::string::npos ||
        (headers.count("content-type") &&
         headers.at("content-type").find("image/") != std::string::npos);

    if (!isLarge && !isImage) {
      entry.data = response;
      updateLruAndPrune(url, response->size());
    } else {
      LOG_D("NetworkManager", "Data for {} is {} ({:.1f} MB), skipping RAM cache",
            url, isLarge ? "large" : "an image",
            response->size() / 1024.0 / 1024.0);
      removeFromLru(url);
      // Metadata-only entries still need LRU tracking for eviction
      lru_.push_front(url);
      // Check entry-count limit even for metadata-only entries
      while ((int)cache_.size() > (int)MAX_CACHE_ENTRIES && !lru_.empty()) {
        std::string oldest = lru_.back();
        auto it = cache_.find(oldest);
        if (it != cache_.end()) {
          if (it->second.data)
            totalRamBytes_ -= it->second.data->size();
          cache_.erase(it);
        }
        lru_.pop_back();
      }
    }

    cache_[url] = entry;
    if (!cacheDir_.empty()) {
      saveToDisk(url, entry, *response);
    }
  }

  callback(std::move(response));
}
#endif

NetworkManager::NetworkManager(const std::filesystem::path &cacheDir)
    : cacheDir_(cacheDir) {
  if (!cacheDir_.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(cacheDir_, ec);
    if (!ec) {
      // loadCache() does directory iteration + per-file I/O which is expensive
      // on SD cards (RPi) and Windows (NTFS + AV scanning).  Run it on a
      // worker thread so the startup thread is not blocked.  inflight_ is
      // pre-incremented here (before the thread starts) so the destructor's
      // wait correctly stalls if the thread has not finished yet.
      // cacheLoadDone_ is set false so waitForCacheLoad() blocks until the
      // thread signals completion — preventing a race where DashboardContext
      // provider fetchAsync() calls arrive before the cache index is ready.
      {
        std::lock_guard<std::mutex> lk(inflightMutex_);
        ++inflight_;
      }
      {
        std::lock_guard<std::mutex> lk(cacheLoadMutex_);
        cacheLoadDone_ = false;
      }
      std::thread([this, alive = alive_]() {
        {
          std::lock_guard<std::mutex> lk(inflightMutex_);
          if (!alive->load(std::memory_order_relaxed)) {
            if (--inflight_ == 0) inflightCv_.notify_all();
            // Signal done (thread aborted) so waitForCacheLoad() doesn't hang
            std::lock_guard<std::mutex> lk2(cacheLoadMutex_);
            cacheLoadDone_ = true;
            cacheLoadCv_.notify_all();
            return;
          }
        }
        loadCache();
        {
          std::lock_guard<std::mutex> lk(cacheLoadMutex_);
          cacheLoadDone_ = true;
        }
        cacheLoadCv_.notify_all();
        {
          std::lock_guard<std::mutex> lk(inflightMutex_);
          if (--inflight_ == 0) inflightCv_.notify_all();
        }
      }).detach();
    } else {
      LOG_E("NetworkManager", "Failed to create cache dir {}: {}",
            cacheDir_.string(), ec.message());
    }
  }
#ifdef __ANDROID__
  buildAndroidCaBundle();
#endif
}

#ifdef __ANDROID__
void NetworkManager::buildAndroidCaBundle() {
  if (cacheDir_.empty()) return;

  androidCaBundlePath_ = (cacheDir_ / "android-ca-bundle.pem").string();

  // Reuse the bundle unless the system cert directory is newer (OS update rotated CAs).
  const char *certDir = "/system/etc/security/cacerts/";
  std::error_code ec;
  auto bundleTime = std::filesystem::last_write_time(androidCaBundlePath_, ec);
  if (!ec) {
    auto dirTime = std::filesystem::last_write_time(certDir, ec);
    if (!ec && dirTime <= bundleTime) return;
  }

  // Android stores each trusted CA as a separate PEM file in this directory.
  // mbedTLS requires a single concatenated bundle via CURLOPT_CAINFO.
  DIR *dir = opendir(certDir);
  if (!dir) {
    LOG_W("NetworkManager", "Android CA dir not accessible: {}", certDir);
    androidCaBundlePath_.clear();
    return;
  }

  std::ofstream out(androidCaBundlePath_);
  if (!out) {
    closedir(dir);
    LOG_W("NetworkManager", "Cannot write Android CA bundle to {}", androidCaBundlePath_);
    androidCaBundlePath_.clear();
    return;
  }

  struct dirent *ent;
  int count = 0;
  while ((ent = readdir(dir)) != nullptr) {
    if (ent->d_name[0] == '.') continue;
    std::ifstream cert(std::string(certDir) + ent->d_name);
    if (cert) {
      out << cert.rdbuf() << "\n";
      ++count;
    }
  }
  closedir(dir);

  LOG_I("NetworkManager", "Built Android CA bundle: {} certs -> {}", count, androidCaBundlePath_);
}
#endif

std::string NetworkManager::hashUrl(const std::string &url) {
  unsigned long hash = 5381;
  for (char c : url)
    hash = ((hash << 5) + hash) + c;

  std::stringstream ss;
  ss << std::hex << hash;
  return ss.str();
}

void NetworkManager::saveToDisk(const std::string &url, const CacheEntry &entry,
                                const std::string &data) {
  if (cacheDir_.empty())
    return;

  const std::string *fullDataPtr = &data;
  if (data.empty()) {
    if (entry.data)
      fullDataPtr = entry.data.get();
    else
      return;
  }

  std::string filename = hashUrl(url);
  std::filesystem::path p = cacheDir_ / filename;
  std::ofstream ofs(p, std::ios::binary);
  if (ofs) {
    // Write a simple header
    ofs << "HamClockCache/1.1\n";
    ofs << entry.timestamp << "\n";
    ofs << entry.serverTime << "\n";
    ofs << url << "\n";
    ofs << entry.lastModified << "\n";
    ofs << entry.etag << "\n";
    ofs.write(fullDataPtr->data(), fullDataPtr->size());
  }
}

void NetworkManager::waitForCacheLoad() {
  std::unique_lock<std::mutex> lk(cacheLoadMutex_);
  cacheLoadCv_.wait(lk, [this] { return cacheLoadDone_; });
}

void NetworkManager::loadCache() {
  if (cacheDir_.empty())
    return;

  for (const auto &entry : std::filesystem::directory_iterator(cacheDir_)) {
    if (entry.is_regular_file()) {
      std::ifstream ifs(entry.path(), std::ios::binary);
      if (ifs) {
        std::string line;
        if (std::getline(ifs, line)) {
          bool v11 = (line == "HamClockCache/1.1");
          if (!v11 && line != "HamClockCache/1.0")
            continue; // Skip old/invalid

          try {
            if (!std::getline(ifs, line))
              continue;
            std::time_t ts = std::stoll(line);

            // Prune cache files older than 30 days based on their internal timestamp
            std::time_t now = std::time(nullptr);
            if (now - ts > 30 * 24 * 3600) {
              ifs.close();
              std::error_code ec;
              std::filesystem::remove(entry.path(), ec);
              LOG_D("NetworkManager", "Pruned old cache file on startup: {}",
                    entry.path().filename().string());
              continue;
            }

            std::time_t serverTs = 0;
            if (v11) {
              if (!std::getline(ifs, line))
                continue;
              serverTs = std::stoll(line);
            }

            std::string url;
            if (!std::getline(ifs, url))
              continue;

            std::string lm;
            if (!std::getline(ifs, lm))
              continue;

            std::string etag;
            if (!std::getline(ifs, etag))
              continue;

            // Load entry metadata from disk. Cache validity is controlled by
            // cacheAgeSeconds parameter in fetchAsync, not by disk file age.
            std::lock_guard<std::mutex> lock(cacheMutex_);
            CacheEntry ce;
            ce.data = nullptr;
            ce.timestamp = ts;
            ce.serverTime = serverTs;
            ce.lastModified = lm;
            ce.etag = etag;
            cache_[url] = ce;
            lru_.push_front(url);
            } catch (const std::exception &ex) {
            LOG_W("NetworkManager", "Cache parse error for {}: {}",
                  entry.path().string(), ex.what());
          }
        }
      }
    }
  }

  // Enforce MAX_CACHE_ENTRIES limit on loaded metadata to prevent unbounded growth
  {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    while ((int)cache_.size() > (int)MAX_CACHE_ENTRIES && !lru_.empty()) {
      std::string oldest = lru_.back();
      auto it = cache_.find(oldest);
      if (it != cache_.end()) {
        cache_.erase(it);
        LOG_D("NetworkManager", "loadCache: Evicted excess entry {} (cache now {} entries)",
              oldest, cache_.size());
      }
      lru_.pop_back();
    }
  }
}

std::string NetworkManager::getLocalIP() {
#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
  ULONG bufLen = 15000;
  std::vector<BYTE> buf(bufLen);
  PIP_ADAPTER_ADDRESSES pAddrs = nullptr;
  ULONG ret;
  for (int attempt = 0; attempt < 2; ++attempt) {
    buf.resize(bufLen);
    pAddrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buf.data());
    ret = GetAdaptersAddresses(AF_INET,
                               GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                   GAA_FLAG_SKIP_DNS_SERVER,
                               nullptr, pAddrs, &bufLen);
    if (ret != ERROR_BUFFER_OVERFLOW)
      break;
  }
  if (ret != NO_ERROR)
    return "--";

  for (PIP_ADAPTER_ADDRESSES a = pAddrs; a; a = a->Next) {
    if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
      continue;
    if (a->IfType == IF_TYPE_TUNNEL)
      continue;
    if (a->OperStatus != IfOperStatusUp)
      continue;
    for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u;
         u = u->Next) {
      auto *sa = u->Address.lpSockaddr;
      if (sa->sa_family != AF_INET)
        continue;
      char ipBuf[INET_ADDRSTRLEN];
      auto *sin = reinterpret_cast<struct sockaddr_in *>(sa);
      if (inet_ntop(AF_INET, &sin->sin_addr, ipBuf, sizeof(ipBuf))) {
        return std::string(ipBuf);
      }
    }
  }
  return "--";
#elif defined(__EMSCRIPTEN__)
  return "WASM";
#else
  struct ifaddrs *addrs = nullptr;
  if (getifaddrs(&addrs) != 0)
    return "--";
  std::string result = "--";
  for (struct ifaddrs *ifa = addrs; ifa; ifa = ifa->ifa_next) {
    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
      continue;
    if (std::strcmp(ifa->ifa_name, "lo") == 0)
      continue;
    auto *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
    result = ip;
    break;
  }
  freeifaddrs(addrs);
  return result;
#endif
}

void NetworkManager::updateLruAndPrune(const std::string &url, size_t dataSize) {
  // Caller must hold cacheMutex_
  removeFromLru(url);

  lru_.push_front(url);
  totalRamBytes_ += dataSize;

  while (totalRamBytes_ > MAX_RAM_BYTES && !lru_.empty()) {
    std::string oldest = lru_.back();
    auto it = cache_.find(oldest);
    if (it != cache_.end()) {
      if (it->second.data) {
        totalRamBytes_ -= it->second.data->size();
      }
      cache_.erase(it);
      LOG_D("NetworkManager", "LRU: Evicted entry {} to save RAM (disk cache preserves metadata)",
            oldest);
    }
    lru_.pop_back();
  }

  // Evict oldest entries if cache has grown too large (metadata bloat from disk-loaded entries)
  while ((int)cache_.size() > (int)MAX_CACHE_ENTRIES && !lru_.empty()) {
    std::string oldest = lru_.back();
    auto it = cache_.find(oldest);
    if (it != cache_.end()) {
      if (it->second.data) {
        totalRamBytes_ -= it->second.data->size();
      }
      cache_.erase(it);
      LOG_D("NetworkManager", "Entry limit: Evicted entire entry for {} (cache now {} entries)",
            oldest, cache_.size());
    }
    lru_.pop_back();
  }
}

void NetworkManager::removeFromLru(const std::string &url) {
  // Caller must hold cacheMutex_
  auto it = std::find(lru_.begin(), lru_.end(), url);
  if (it != lru_.end()) {
    auto cit = cache_.find(url);
    if (cit != cache_.end() && cit->second.data) {
      totalRamBytes_ -= cit->second.data->size();
    }
    lru_.erase(it);
  }
}

void NetworkManager::pruneStaleCache(int maxAgeSeconds) {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  std::time_t now = std::time(nullptr);
  int count = 0;
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (now - it->second.timestamp > maxAgeSeconds) {
      if (!cacheDir_.empty()) {
        std::error_code ec;
        std::filesystem::remove(cacheDir_ / hashUrl(it->first), ec);
      }
      auto lruIt = std::find(lru_.begin(), lru_.end(), it->first);
      if (lruIt != lru_.end())
        lru_.erase(lruIt);
      if (it->second.data) {
        totalRamBytes_ -= it->second.data->size();
      }
      it = cache_.erase(it);
      count++;
    } else {
      ++it;
    }
  }
  if (count > 0) {
    LOG_I("NetworkManager", "Pruned {} stale cache entries from memory and disk",
          count);
  }
}

void NetworkManager::dumpCacheStats() const {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  size_t withData = 0, metadataOnly = 0, totalMetadataBytes = 0;
  for (const auto& [url, entry] : cache_) {
    if (entry.data) {
      withData++;
    } else {
      metadataOnly++;
      totalMetadataBytes += url.size() + entry.lastModified.size() + entry.etag.size();
    }
  }
  LOG_I("NetworkManager",
    "Cache stats: %zu w/data, %zu metadata-only (~%zu KB), LRU: %zu, RAM: %zu MB, Entries: %zu/%zu",
    withData, metadataOnly, totalMetadataBytes / 1024, lru_.size(),
    totalRamBytes_ / 1024 / 1024, cache_.size(), MAX_CACHE_ENTRIES);
}

void NetworkManager::clearRamCache() {
  std::lock_guard<std::mutex> lock(cacheMutex_);
  cache_.clear();
  lru_.clear();
  totalRamBytes_ = 0;
  LOG_I("NetworkManager", "Cleared all cache entries and RAM payloads");
}

