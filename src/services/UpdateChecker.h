#pragma once

#include <mutex>
#include <string>

class NetworkManager;

// Polls the GitHub Releases API once at startup (and periodically via fetch())
// to detect when a newer version of HamClock-Next is available.
// All methods are thread-safe; fetch() fires an async callback from a worker
// thread and must NOT be called from WASM builds.
class UpdateChecker {
public:
  explicit UpdateChecker(NetworkManager &net);

  // Kick off (or refresh) the GitHub release check.
  // Results are cached for 6 hours by NetworkManager.
  void fetch();

  // Returns true if a release tag different from HAMCLOCK_VERSION was found.
  bool updateAvailable() const;

  // Returns the latest tag_name from GitHub (v-prefix stripped), or "" if
  // not yet fetched.
  std::string latestVersion() const;

private:
  NetworkManager &net_;
  mutable std::mutex mutex_;
  bool updateAvailable_{false};
  std::string latestVersion_;
};
