#include "UpdateChecker.h"
#include "../core/Logger.h"
#include "../network/NetworkManager.h"
#include "../core/Constants.h"
#include <nlohmann/json.hpp>

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

UpdateChecker::UpdateChecker(NetworkManager &net) : net_(net) {}

void UpdateChecker::fetch() {
  net_.fetchAsync(
      kApiUrl,
      [](std::string data) {
        if (data.empty())
          return;

        SDL_Event ev;
        SDL_zero(ev);
        ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_UPDATE_DATA_READY;
        ev.user.data1 = new std::string(std::move(data));
        SDL_PushEvent(&ev);
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

      std::lock_guard<std::mutex> lock(mutex_);
      latestVersion_ = tag;
      releaseNotes_ = notes;
      updateAvailable_ = available;

      if (available) {
        LOG_I("UpdateChecker", "Update available: {} → {} (current: {})",
              current, tag, current);
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
