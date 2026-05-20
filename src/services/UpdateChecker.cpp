#ifndef __EMSCRIPTEN__

#include "UpdateChecker.h"
#include "../core/Logger.h"
#include "../network/NetworkManager.h"
#include "../core/Constants.h"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <cstring>

static constexpr const char *kApiUrl =
    "https://api.github.com/repos/k4drw/hamclock-next/releases/latest";

// Cache the GitHub response for 6 hours to avoid hammering the API.
static constexpr int kCacheSeconds = 6 * 3600;

// Helper to pack version strings (X.Y.Z[Suffix]) into a 32-bit integer for
// comparison. Top 3 bytes hold up to 3 numeric parts. Lowest byte holds the
// suffix code (255 for final release).
static uint32_t versionToUint32(std::string v) {
  if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
    v = v.substr(1);
  }

  uint32_t result = 0;
  int shift = 24;

  size_t start = 0;
  std::string suffix = "";

  for (int i = 0; i < 3; ++i) {
    if (start >= v.length())
      break;

    size_t end = v.find('.', start);
    std::string part = (end == std::string::npos)
                           ? v.substr(start)
                           : v.substr(start, end - start);

    size_t pos = 0;
    uint32_t num = 0;
    try {
      num = std::stoul(part, &pos);
    } catch (...) {
    }

    result |= ((num & 0xFF) << shift);
    shift -= 8;

    if (pos < part.length()) {
      suffix = part.substr(pos);
    }

    if (end == std::string::npos)
      break;
    start = end + 1;
  }

  uint32_t suffixVal = 255;
  if (!suffix.empty()) {
    size_t firstDigit = suffix.find_first_of("0123456789");
    if (firstDigit != std::string::npos) {
      try {
        suffixVal = std::stoul(suffix.substr(firstDigit));
      } catch (...) {
        suffixVal = 0;
      }
    } else {
      suffixVal = 0;
    }
  }

  result |= (suffixVal & 0xFF);
  return result;
}

// Returns true if remote is strictly NEWER than local.
static bool isVersionNewer(std::string remote, std::string local) {
  return versionToUint32(remote) > versionToUint32(local);
}

// --- Asset matching ---------------------------------------------------------
// Given the compile-time install type, arch, and build variant, score each
// release asset name and return the URL of the best match (or "" if none).

static std::string pickDownloadUrl(const nlohmann::json &assets) {
  const std::string type    = HAMCLOCK_INSTALL_TYPE;   // DEB, RPM, BIN, WASM
  const std::string arch    = HAMCLOCK_ARCH;            // x86_64, aarch64, arm, …
  const std::string variant = HAMCLOCK_BUILD_VARIANT;  // unified, fb0, ""

  if (type == "WASM")
    return "";  // browser reload, no artifact to download

  // Translate cmake arch to Debian arch naming used in asset filenames
  std::string debArch;
  if (arch == "x86_64")        debArch = "amd64";
  else if (arch == "aarch64")  debArch = "arm64";
  else if (arch == "arm" || arch.rfind("armv", 0) == 0)
                               debArch = "armhf";

  // Helper: case-insensitive substring check
  auto contains = [](const std::string &s, const std::string &sub) -> bool {
    if (sub.empty()) return true;
    std::string sl = s, subl = sub;
    std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
    std::transform(subl.begin(), subl.end(), subl.begin(), ::tolower);
    return sl.find(subl) != std::string::npos;
  };

  std::string bestUrl;
  int bestScore = -1;

  for (const auto &asset : assets) {
    if (!asset.contains("name") || !asset.contains("browser_download_url"))
      continue;
    const std::string name = asset["name"];
    const std::string url  = asset["browser_download_url"];
    int score = 0;

    if (type == "RPM" || (type == "BIN" && !debArch.empty())) {
      // For RPM: prefer .rpm files
      if (type == "RPM" && contains(name, ".rpm")) {
        score = 1;
        if (!arch.empty() && (contains(name, arch) || contains(name, debArch)))
          score = 2;
      }
      // For BIN (source-compiled): fall through to DEB matching below
    }

    if ((type == "DEB" || type == "BIN") && !debArch.empty()) {
      if (contains(name, ".deb") && contains(name, debArch)) {
        score = 2;
        // Prefer variant match if we have one baked in
        if (!variant.empty() && contains(name, variant))
          score = 3;
      }
    }

    if (score > bestScore) {
      bestScore = score;
      bestUrl = url;
    }
  }

  return bestUrl;
}

// --- libcurl download helpers -----------------------------------------------

static size_t writeData(void *ptr, size_t size, size_t nmemb, void *userdata) {
  auto *file = static_cast<std::ofstream *>(userdata);
  file->write(static_cast<const char *>(ptr),
              static_cast<std::streamsize>(size * nmemb));
  return size * nmemb;
}

// userdata points to a DownloadContext; returning non-zero aborts the transfer.
static int progressCallback(void *userdata, curl_off_t dlTotal,
                             curl_off_t dlNow, curl_off_t, curl_off_t) {
  auto *ctx = static_cast<UpdateChecker::DownloadContext *>(userdata);
  if (ctx->cancel.load(std::memory_order_acquire))
    return 1;  // abort — destructor has signalled cancellation
  if (dlTotal > 0)
    ctx->progress.store(static_cast<float>(dlNow) / static_cast<float>(dlTotal));
  return 0;
}

// ---------------------------------------------------------------------------

UpdateChecker::UpdateChecker(NetworkManager &net) : net_(net) {}

UpdateChecker::~UpdateChecker() {
  // Signal any in-progress download to abort via the curl progress callback,
  // then wait for the thread to decrement inflight_ and exit cleanly.
  {
    std::lock_guard<std::mutex> lk(inflightMutex_);
    if (dlCtx_)
      dlCtx_->cancel.store(true, std::memory_order_release);
  }
  std::unique_lock<std::mutex> lk(inflightMutex_);
  inflightCv_.wait(lk, [this] { return inflight_ == 0; });
}

void UpdateChecker::fetch() {
  net_.fetchAsync(
      kApiUrl,
      [](std::string data) {
        if (data.empty())
          return;

        SDL_Event ev;
        SDL_zero(ev);
        ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_UPDATE_DATA_READY;
        auto *updateData = new std::string(std::move(data));
        ev.user.data1 = updateData;
        if (SDL_PushEvent(&ev) < 0)
          delete updateData;
      },
      kCacheSeconds);
}

void UpdateChecker::onDataReady(const std::string &data) {
  try {
    auto j = nlohmann::json::parse(data);
    const std::string current = HAMCLOCK_VERSION;

    if (j.contains("tag_name")) {
      std::string tag = j["tag_name"];
      if (tag.empty() || (tag[0] != 'v' && tag[0] != 'V')) {
        tag = "v" + tag;
      }
      std::string notes = j.value("body", "");
      const bool available = isVersionNewer(tag, current);

      // Pick the best download URL from the release assets
      std::string dlUrl;
      if (available && j.contains("assets") && j["assets"].is_array()) {
        dlUrl = pickDownloadUrl(j["assets"]);
      }

      std::lock_guard<std::mutex> lock(mutex_);
      latestVersion_ = tag;
      releaseNotes_ = notes;
      updateAvailable_ = available;
      downloadUrl_ = dlUrl;

      if (available) {
        LOG_I("UpdateChecker", "Update available: {} → {} (current: {})",
              current, tag, current);
        if (!dlUrl.empty())
          LOG_D("UpdateChecker", "Download URL: {}", dlUrl);
        else
          LOG_D("UpdateChecker", "No matching download asset for this build");
      } else {
        LOG_D("UpdateChecker", "Up to date ({})", current);
      }
    } else {
      LOG_D("UpdateChecker", "Up to date ({})", current);
    }
  } catch (...) {
    LOG_W("UpdateChecker", "Failed to parse GitHub releases response");
  }
}

bool UpdateChecker::updateAvailable() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return updateAvailable_;
}

std::string UpdateChecker::latestVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return latestVersion_;
}

std::string UpdateChecker::releaseNotes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return releaseNotes_;
}

std::string UpdateChecker::downloadUrl() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return downloadUrl_;
}

UpdateChecker::DownloadState UpdateChecker::downloadState() const {
  return downloadState_.load();
}

float UpdateChecker::downloadProgress() const {
  std::shared_ptr<DownloadContext> ctx;
  {
    std::lock_guard<std::mutex> lk(inflightMutex_);
    ctx = dlCtx_;
  }
  return ctx ? ctx->progress.load() : 0.0f;
}

std::string UpdateChecker::downloadedPath() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return downloadedPath_;
}

void UpdateChecker::startDownload() {
  // Guard against double-click / re-entry
  DownloadState expected = DownloadState::Idle;
  if (!downloadState_.compare_exchange_strong(expected, DownloadState::InProgress)) {
    // Also allow retry from Failed state
    expected = DownloadState::Failed;
    if (!downloadState_.compare_exchange_strong(expected, DownloadState::InProgress))
      return;
  }

  std::string url;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    url = downloadUrl_;
  }

  if (url.empty()) {
    downloadState_.store(DownloadState::Failed);
    return;
  }

  // Create a fresh shared context; the thread holds a copy so progress and
  // cancel remain valid even if the destructor races with thread startup.
  std::shared_ptr<DownloadContext> ctx;
  {
    std::lock_guard<std::mutex> lk(inflightMutex_);
    dlCtx_ = std::make_shared<DownloadContext>();
    ctx = dlCtx_;
    ++inflight_;
  }

  // Determine temp file extension from URL
  auto ext = std::filesystem::path(url).extension().string();
  if (ext.empty()) ext = ".bin";
  auto tmpPath = std::filesystem::temp_directory_path() /
                 ("hamclock-next-update" + ext);

  std::thread([this, url, tmpPath, ctx]() {
    std::ofstream outFile(tmpPath, std::ios::binary | std::ios::trunc);
    if (!outFile) {
      LOG_W("UpdateChecker", "Cannot open temp file for download: {}",
            tmpPath.string());
      downloadState_.store(DownloadState::Failed);
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (--inflight_ == 0) inflightCv_.notify_all();
      return;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
      LOG_W("UpdateChecker", "curl_easy_init failed");
      downloadState_.store(DownloadState::Failed);
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (--inflight_ == 0) inflightCv_.notify_all();
      return;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, ctx.get());
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  // 5-minute cap
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
                     "HamClock-Next/" HAMCLOCK_VERSION);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    outFile.close();

    if (res == CURLE_OK) {
      LOG_I("UpdateChecker", "Download complete: {}", tmpPath.string());
      std::lock_guard<std::mutex> lock(mutex_);
      downloadedPath_ = tmpPath.string();
      downloadState_.store(DownloadState::Complete);
    } else {
      LOG_W("UpdateChecker", "Download failed: {}", curl_easy_strerror(res));
      std::filesystem::remove(tmpPath);
      downloadState_.store(DownloadState::Failed);
    }

    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      if (--inflight_ == 0) inflightCv_.notify_all();
    }
  }).detach();
}

#endif // __EMSCRIPTEN__
