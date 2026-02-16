#include "NetworkManager.h"
#include "../core/Logger.h"

#include <curl/curl.h>

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
        LOG_T(
            "NetworkManager",
            "Memory record found but no data (too large), checking disk for {}",
            url);
        // Data was too large for RAM cache, check disk.
        // For simplicity, we just let the thread handle it.
      }
    }
  }

  std::thread([this, url, callback = std::move(callback), hasCache, cached]() {
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamClock-Next/1.0");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

// On Linux with static mbedTLS, we often need to point CURL to the CA
// bundle. However, for system libcurl (dynamic), this is usually automatic.
// We remove the hardcoded path to let libcurl decide.
#ifdef __linux__
    if (std::filesystem::exists("/etc/ssl/certs/ca-certificates.crt")) {
      curl_easy_setopt(curl, CURLOPT_CAINFO,
                       "/etc/ssl/certs/ca-certificates.crt");
    } else if (std::filesystem::exists("/etc/pki/tls/certs/ca-bundle.crt")) {
      curl_easy_setopt(curl, CURLOPT_CAINFO,
                       "/etc/pki/tls/certs/ca-bundle.crt");
    } else if (std::filesystem::exists("/etc/ssl/ca-bundle.pem")) {
      curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/ca-bundle.pem");
    }
#endif

    // If we have cache, do a HEAD request first to verify
    if (hasCache && !cached.lastModified.empty()) {
      curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
      CURLcode res = curl_easy_perform(curl);
      if (res == CURLE_OK) {
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        if (responseCode >= 200 && responseCode < 300) {
          if (headers.count("last-modified") &&
              headers.at("last-modified") == cached.lastModified) {
            LOG_T("NetworkManager", "Cache validated (HEAD) for {}", url);
            // Still valid! Update timestamp and return cached
            {
              std::lock_guard<std::mutex> lock(cacheMutex_);
              cache_[url].timestamp = std::time(nullptr);
              saveToDisk(url, cache_[url], cached.data);
            }
            curl_easy_cleanup(curl);
            callback(cached.data);
            return;
          }
        }
      }
      // Reset for full GET if HEAD failed or was different
      curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    LOG_D("NetworkManager", "Fetching from network: {}", url);
    CURLcode res = curl_easy_perform(curl);

    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      LOG_E("NetworkManager", "Fetch failed for {}: {}", url,
            curl_easy_strerror(res));
      callback("");
      return;
    }

    if (responseCode != 200) {
      LOG_E("NetworkManager", "HTTP error {} for {}", responseCode, url);
      callback("");
      return;
    }

    // Update cache on success
    {
      std::lock_guard<std::mutex> lock(cacheMutex_);
      std::time_t now = std::time(nullptr);
      CacheEntry entry;
      entry.timestamp = now;
      if (headers.count("last-modified"))
        entry.lastModified = headers.at("last-modified");
      if (headers.count("etag"))
        entry.etag = headers.at("etag");

      // Memory-Optimization: Only store small data in RAM cache.
      // Large maps (50MB+) should only live on disk.
      bool isLarge = response.size() > 512 * 1024; // 512 KB
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
  }).detach();
}

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
    ofs << "HamClockCache/1.0\n";
    ofs << entry.timestamp << "\n";
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
          if (line != "HamClockCache/1.0")
            continue; // Skip old/invalid

          try {
            if (!std::getline(ifs, line))
              continue;
            std::time_t ts = std::stoll(line);

            std::string url;
            if (!std::getline(ifs, url))
              continue;

            std::string lm;
            if (!std::getline(ifs, lm))
              continue;

            std::string etag;
            if (!std::getline(ifs, etag))
              continue;

            // Read rest of file as data
            std::string data((std::istreambuf_iterator<char>(ifs)),
                             (std::istreambuf_iterator<char>()));

            std::lock_guard<std::mutex> lock(cacheMutex_);
            cache_[url] = {data, ts, lm, etag};
          } catch (...) {
          }
        }
      }
    }
  }
}
