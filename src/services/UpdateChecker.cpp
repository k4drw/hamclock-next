#include "UpdateChecker.h"
#include "../core/Logger.h"
#include "../network/NetworkManager.h"
#include "../core/Constants.h"
#include <nlohmann/json.hpp>

static constexpr const char *kApiUrl =
    "https://api.github.com/repos/k4drw/hamclock-next/releases/latest";

// Cache the GitHub response for 6 hours to avoid hammering the API.
static constexpr int kCacheSeconds = 6 * 3600;

// Helper to compare version strings (X.Y.Z[Suffix])
// Returns true if remote is strictly NEWER than local.
static bool isVersionNewer(std::string remote, std::string local) {
  // First, normalize by removing dots and 'v' prefix to check for equivalence
  // (e.g., v0.9.5 internal vs v0.95 release)
  auto normalize = [](std::string v) {
    v.erase(std::remove(v.begin(), v.end(), '.'), v.end());
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
      v = v.substr(1);
    }
    return v;
  };
  if (normalize(remote) == normalize(local)) {
    return false;
  }

  auto stripVOwn = [](std::string v) {
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
      return v.substr(1);
    }
    return v;
  };
  remote = stripVOwn(remote);
  local = stripVOwn(local);

  auto split = [](const std::string &v) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : v) {
      if (c == '.') {
        parts.push_back(current);
        current = "";
      } else {
        current += c;
      }
    }
    parts.push_back(current);
    return parts;
  };

  auto r_parts = split(remote);
  auto l_parts = split(local);

  size_t n = std::max(r_parts.size(), l_parts.size());
  for (size_t i = 0; i < n; ++i) {
    std::string r_s = (i < r_parts.size()) ? r_parts[i] : "0";
    std::string l_s = (i < l_parts.size()) ? l_parts[i] : "0";

    // Extract numeric part
    size_t r_pos = 0, l_pos = 0;
    int r_n = 0, l_n = 0;
    try {
      r_n = std::stoi(r_s, &r_pos);
    } catch (...) {
    }
    try {
      l_n = std::stoi(l_s, &l_pos);
    } catch (...) {
    }

    if (r_n > l_n)
      return true;
    if (r_n < l_n)
      return false;

    // Numbers are equal, compare suffixes (e.g. "6B" vs "60")
    // Simple string comparison for suffixes if present
    std::string r_suffix = r_s.substr(r_pos);
    std::string l_suffix = l_s.substr(l_pos);
    if (r_suffix != l_suffix) {
      // "B" (beta) is usually considered older than "" (release)
      if (r_suffix.empty())
        return true; // Remote is release, local is beta/suffix
      if (l_suffix.empty())
        return false; // Local is release, remote is beta/suffix
      return r_suffix > l_suffix;
    }
  }

  return false;
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
