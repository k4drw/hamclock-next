#include "NetworkManager.h"

#include <curl/curl.h>

#include <cstdio>
#include <thread>

#include <filesystem>
#include <fstream>

#include <sstream>

static size_t writeCallback(char *ptr, size_t size, size_t nmemb,
                            void *userdata) {
  auto *response = static_cast<std::string *>(userdata);
  response->append(ptr, size * nmemb);
  return size * nmemb;
}

// Basic in-memory cache to prevent accidental tight-loop fetches
void NetworkManager::fetchAsync(const std::string &url,
                                std::function<void(std::string)> callback,
                                int cacheAgeSeconds, bool force) {
  // Check cache first
  if (!force) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cache_.find(url);
    if (it != cache_.end()) {
      std::time_t now = std::time(nullptr);
      if (now - it->second.timestamp < cacheAgeSeconds) {
        // Return cached copy immediately (on calling thread? Or async?
        // Async is safer to avoid blocking UI if callback does heavy work,
        // but for now let's just callback directly to keep it simple,
        // assuming callback is fast or posts to main thread).
        // To be safe and consistent with "Async", we should probably spawn a
        // thread or post a task. But for this simple manager, direct callback
        // is okay provided the caller handles it. However, the caller expects
        // async. Let's spawn a quick thread to invoke callback.
        std::string data = it->second.data;
        std::thread([cb = std::move(callback), data = std::move(data)]() {
          cb(data);
        }).detach();
        return;
      }
    }
  }

  std::thread([this, url, callback = std::move(callback)]() {
    CURL *curl = curl_easy_init();
    if (!curl) {
      std::fprintf(stderr, "NetworkManager: curl_easy_init failed\n");
      callback("");
      return;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Pretend to be a browser to avoid some strict anti-bot filters
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "HamClock-Next/1.0");

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
      std::fprintf(stderr, "NetworkManager: fetch failed for %s: %s\n",
                   url.c_str(), curl_easy_strerror(res));
      callback("");
      return;
    }

    // Update cache on success
    {
      std::lock_guard<std::mutex> lock(cacheMutex_);
      std::time_t now = std::time(nullptr);
      CacheEntry entry = {response, now};
      cache_[url] = entry;

      // Persist to disk if cache dir is set
      if (!cacheDir_.empty()) {
        saveToDisk(url, entry);
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
      std::fprintf(stderr,
                   "NetworkManager: failed to create cache dir %s: %s\n",
                   cacheDir_.c_str(), ec.message().c_str());
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

void NetworkManager::saveToDisk(const std::string &url,
                                const CacheEntry &entry) {
  if (cacheDir_.empty())
    return;

  std::string filename = hashUrl(url);
  std::filesystem::path p = cacheDir_ / filename;
  std::ofstream ofs(p, std::ios::binary);
  if (ofs) {
    ofs << entry.timestamp << "\n";
    ofs << url << "\n";
    ofs << entry.data;
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
          try {
            std::time_t ts = std::stoll(line);
            std::string url;
            if (std::getline(ifs, url)) {
              // Read rest of file
              std::string data((std::istreambuf_iterator<char>(ifs)),
                               (std::istreambuf_iterator<char>()));

              std::lock_guard<std::mutex> lock(cacheMutex_);
              cache_[url] = {data, ts};
            }
          } catch (...) {
          }
        }
      }
    }
  }
}
