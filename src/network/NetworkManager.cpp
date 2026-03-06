#include "NetworkManager.h"
#include "../core/Logger.h"

#ifndef __EMSCRIPTEN__
#include <curl/curl.h>
#endif

#include <thread>

#include <filesystem>
#include <fstream>

#include <cctype> // For std::tolower
#include <sstream>
#include <unordered_map> // For header parsing

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

static const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

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
  while (out.size() % 4)
    out.push_back('=');
  return out;
}

void NetworkManager::setHubConfig(HubMode mode, const std::string &ip,
                                  int port) {
  std::lock_guard<std::mutex> lk(hubMutex_);
  hubMode_ = mode;
  hubIp_ = ip;
  hubPort_ = port;
}

std::string NetworkManager::fetchFromHubSync(const std::string &hubUrl) {
#ifdef __EMSCRIPTEN__
  return "";
#else
  std::string result;
  CURL *curl = curl_easy_init();
  if (!curl)
    return "";
  curl_easy_setopt(curl, CURLOPT_URL, hubUrl.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
  CURLcode rc = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_easy_cleanup(curl);
  if (rc != CURLE_OK || httpCode != 200)
    return "";
  return result;
#endif
}

// Basic in-memory cache to prevent accidental tight-loop fetches
void NetworkManager::fetchAsync(const std::string &url,
                                std::function<void(std::string)> callback,
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
      if (!cached.data.empty()) {
        LOG_T("NetworkManager", "Memory cache hit for {}", url);
        callback(cached.data);
        return;
      } else {
        LOG_T("NetworkManager",
              "Memory record found but no data (too large), loading from disk "
              "for {}",
              url);
        std::thread([this, url, callback]() {
          std::filesystem::path p = cacheDir_ / hashUrl(url);
          std::ifstream ifs(p, std::ios::binary);
          std::string data;
          if (ifs) {
            std::string line;
            if (std::getline(ifs, line)) {
              bool v11 = (line == "HamClockCache/1.1");
              int skip = v11 ? 5 : 4;
              for (int i = 0; i < skip; ++i)
                std::getline(ifs, line);
              data.assign((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));
            }
          }
          callback(data);
        }).detach();
        return;
      }
    }
  }

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

  // Capture callback in a heap-allocated wrapper
  struct FetchCtx {
    NetworkManager *mgr;
    std::string url;
    std::function<void(std::string)> cb;
  };
  auto *ctx = new FetchCtx{this, url, callback};
  attr.userData = ctx;

  attr.onsuccess = [](emscripten_fetch_t *fetch) {
    auto *ctx = static_cast<FetchCtx *>(fetch->userData);
    if (fetch->data && fetch->numBytes > 0) {
      std::string response(fetch->data, fetch->numBytes);

      // Update in-memory cache
      {
        std::lock_guard<std::mutex> lock(ctx->mgr->cacheMutex_);
        CacheEntry entry;
        entry.timestamp = std::time(nullptr);
        // We don't have header headers easily in simple fetch on WASM,
        // but we can at least cache the data for small responses.
        if (response.size() < 512 * 1024) {
          entry.data = response;
        }
        ctx->mgr->cache_[ctx->url] = entry;
      }

      ctx->cb(std::move(response));
    } else {
      ctx->cb("");
    }
    delete ctx;
    emscripten_fetch_close(fetch);
  };
  attr.onerror = [](emscripten_fetch_t *fetch) {
    auto *ctx = static_cast<FetchCtx *>(fetch->userData);
    LOG_E("NetworkManager", "WASM fetch failed for {} (status {})", ctx->url,
          fetch->status);
    ctx->cb(""); // empty string = failure
    delete ctx;
    emscripten_fetch_close(fetch);
  };

  emscripten_fetch(&attr, fetchUrl.c_str());
#else
  // --- Hub client proxy ---
  {
    std::lock_guard<std::mutex> lk(hubMutex_);
    if (hubMode_ == HubMode::Client && !hubIp_.empty()) {
      std::string encoded = base64Encode(url);
      std::string hubUrl = "http://" + hubIp_ + ":" + std::to_string(hubPort_) +
                           "/api/hub/fetch?url=" + encoded +
                           "&max_age=" + std::to_string(cacheAgeSeconds);
      
      LOG_D("NetworkManager", "Hub client: Proxying request for {} to Hub at {}", url, hubUrl);

      std::thread([this, hubUrl, url, callback = std::move(callback), hasCache,
                   cached]() mutable {
        std::string body = fetchFromHubSync(hubUrl);
        if (!body.empty()) {
          LOG_D("NetworkManager", "Hub client: Received {} bytes from Hub for {}", body.size(), url);
          {
            std::lock_guard<std::mutex> lock(cacheMutex_);
            CacheEntry entry;
            entry.timestamp = std::time(nullptr);
            if (body.size() < 512 * 1024)
              entry.data = body;
            cache_[url] = entry;
          }
          callback(std::move(body));
          return;
        }
        LOG_D("NetworkManager", "Hub client: Hub miss or error for {}, falling back to direct", url);
        fetchDirect(url, std::move(callback), hasCache, cached);
      }).detach();
      return;
    }
  }
  // --- Direct fetch ---
  std::thread([this, url, callback = std::move(callback), hasCache,
               cached]() mutable {
    fetchDirect(url, std::move(callback), hasCache, cached);
  }).detach();
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

#ifndef __EMSCRIPTEN__
void NetworkManager::fetchDirect(const std::string &url,
                                 std::function<void(std::string)> callback,
                                 bool hasCache, const CacheEntry &cached) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    LOG_E("NetworkManager", "curl_easy_init failed");
    callback("");
    return;
  }

  std::unordered_map<std::string, std::string> headers;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  
  if (url.find("pskreporter.info") != std::string::npos) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamClock/1.0");
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://pskreporter.info/pskmap.html");
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
#ifdef __linux__
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
  struct curl_slist *chunk = NULL;
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

  std::string response;
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

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
    callback("");
    return;
  }

  // Handle 304 Not Modified
  if (responseCode == 304) {
    LOG_D("NetworkManager", "304 Not Modified for {}", url);
    std::string retData = cached.data;
    if (retData.empty()) {
      std::filesystem::path p = cacheDir_ / hashUrl(url);
      std::ifstream ifs(p, std::ios::binary);
      if (ifs) {
        std::string line;
        if (std::getline(ifs, line)) {
          bool v11 = (line == "HamClockCache/1.1");
          int skip = v11 ? 5 : 4;
          for (int i = 0; i < skip; ++i)
            std::getline(ifs, line);
          retData.assign((std::istreambuf_iterator<char>(ifs)),
                         (std::istreambuf_iterator<char>()));
        }
      }
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
    callback("");
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

    bool isLarge = response.size() > 512 * 1024;
    if (!isLarge) {
      entry.data = response;
    } else {
      LOG_D("NetworkManager",
            "Data for {} is large ({:.1f} MB), skipping RAM cache", url,
            response.size() / 1024.0 / 1024.0);
    }

    cache_[url] = entry;
    if (!cacheDir_.empty()) {
      saveToDisk(url, entry, response);
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
      loadCache();
    } else {
      LOG_E("NetworkManager", "Failed to create cache dir {}: {}",
            cacheDir_.string(), ec.message());
    }
  }
}

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

  const std::string &fullData = data.empty() ? entry.data : data;
  if (fullData.empty())
    return;

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
    ofs << fullData;
  }
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

            auto currentPos = ifs.tellg();
            ifs.seekg(0, std::ios::end);
            size_t dataSize = (size_t)ifs.tellg() - (size_t)currentPos;
            ifs.seekg(currentPos, std::ios::beg);

            std::string data;
            if (dataSize <= 512 * 1024) {
              // Read rest of file as data
              data.assign((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));
            }

            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[url] = {std::move(data), ts, serverTs, lm, etag};
          } catch (const std::exception &ex) {
            LOG_W("NetworkManager", "Cache parse error for {}: {}",
                  entry.path().string(), ex.what());
          }
        }
      }
    }
  }
}
