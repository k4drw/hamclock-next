#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
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

  // Destructor signals any in-progress download to abort and waits for the
  // thread to exit so no members are accessed after destruction.
  ~UpdateChecker();

  UpdateChecker(const UpdateChecker &) = delete;
  UpdateChecker &operator=(const UpdateChecker &) = delete;

  // Kick off (or refresh) the GitHub release check.
  // Results are cached for 6 hours by NetworkManager.
  void fetch();

  // Thread-safe method for receiving data from background threads
  void onDataReady(const std::string &raw);

  // Returns true if a release tag different from HAMCLOCK_VERSION was found.
  bool updateAvailable() const;

  // Returns the latest tag_name from GitHub (v-prefix stripped), or "" if
  // not yet fetched.
  std::string latestVersion() const;

  // Returns the release notes (GitHub release body)
  std::string releaseNotes() const;

  // Returns the direct download URL for the best-matching release asset,
  // or "" if no suitable asset was found or update check hasn't run yet.
  std::string downloadUrl() const;

  // Returns system info strings defined at compile time
  std::string installType() const { return HAMCLOCK_INSTALL_TYPE; }
  std::string arch() const { return HAMCLOCK_ARCH; }
  std::string buildVariant() const { return HAMCLOCK_BUILD_VARIANT; }

  // --- In-app download ---

  enum class DownloadState { Idle, InProgress, Complete, Failed };

  // Shared between UpdateChecker and the download thread. Heap-allocated and
  // ref-counted so the thread can safely read/write it after UpdateChecker is
  // destroyed (destructor sets cancel and waits, so this is belt-and-suspenders).
  struct DownloadContext {
    std::atomic<float> progress{0.0f};
    std::atomic<bool>  cancel{false};
  };

  // Kick off a background download of the matched release asset.
  // Safe to call from the UI thread; no-op if already in progress.
  void startDownload();

  DownloadState downloadState() const;
  float downloadProgress() const;        // 0.0–1.0 while InProgress
  std::string downloadedPath() const;    // Non-empty when Complete

private:
  NetworkManager &net_;
  mutable std::mutex mutex_;
  bool updateAvailable_{false};
  std::string latestVersion_;
  std::string releaseNotes_;
  std::string downloadUrl_;

  std::atomic<DownloadState> downloadState_{DownloadState::Idle};
  std::string downloadedPath_;  // guarded by mutex_

  // Download thread lifetime tracking — mirrors FccProvider pattern.
  std::shared_ptr<DownloadContext> dlCtx_;
  mutable std::mutex inflightMutex_;
  std::condition_variable inflightCv_;
  int inflight_{0};
};
