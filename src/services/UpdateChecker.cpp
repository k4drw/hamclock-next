#include "UpdateChecker.h"
#include "../core/Logger.h"
#include "../network/NetworkManager.h"
#include <nlohmann/json.hpp>

static constexpr const char *kApiUrl =
    "https://api.github.com/repos/k4drw/hamclock-next/releases/latest";

// Cache the GitHub response for 6 hours to avoid hammering the API.
static constexpr int kCacheSeconds = 6 * 3600;

UpdateChecker::UpdateChecker(NetworkManager &net) : net_(net) {}

void UpdateChecker::fetch() {
  net_.fetchAsync(
      kApiUrl,
      [this](std::string data) {
        if (data.empty())
          return;
        try {
          auto j = nlohmann::json::parse(data);
          std::string tag = j.value("tag_name", "");
          if (tag.empty())
            return;

          // Strip leading 'v' so "v0.9.6" → "0.9.6"
          if (tag.front() == 'v')
            tag = tag.substr(1);

          const std::string current = HAMCLOCK_VERSION;
          const bool available = (tag != current);

          std::lock_guard<std::mutex> lock(mutex_);
          latestVersion_ = tag;
          updateAvailable_ = available;

          if (available) {
            LOG_I("UpdateChecker",
                  "Update available: {} → {} (current: {})",
                  current, tag, current);
          } else {
            LOG_D("UpdateChecker", "Up to date ({})", current);
          }
        } catch (...) {
          LOG_W("UpdateChecker", "Failed to parse GitHub releases response");
        }
      },
      kCacheSeconds);
}

bool UpdateChecker::updateAvailable() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return updateAvailable_;
}

std::string UpdateChecker::latestVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return latestVersion_;
}
