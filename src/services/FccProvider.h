#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/// A single FCC ULS license entry parsed from the search results page.
struct FccLicense {
  std::string callsign;
  std::string service;
  std::string status;
  std::string expiry; // YYYY-MM-DD format
};

/// Provides FCC ULS license lookups by FRN (FCC Registration Number).
/// Uses the same 2-step GET/POST session approach as the FCC ULS web UI.
/// Results are delivered via callback on a background thread.
///
/// Shutdown: the destructor flips alive_ and blocks until every detached
/// lookup thread that already passed the alive_ check has finished.  Mirrors
/// the NetworkManager pattern so the provider (and its callback state) never
/// outlives threads that could still invoke the callback.
class FccProvider {
public:
  using ResultCb = std::function<void(std::vector<FccLicense>)>;

  FccProvider() = default;

  ~FccProvider() {
    {
      std::lock_guard<std::mutex> lk(inflightMutex_);
      alive_->store(false, std::memory_order_release);
    }
    std::unique_lock<std::mutex> lk(inflightMutex_);
    inflightCv_.wait(lk, [this] { return inflight_ == 0; });
  }

  FccProvider(const FccProvider &) = delete;
  FccProvider &operator=(const FccProvider &) = delete;

  /// Look up all licenses under the given FRN.
  /// @param frn   FRN string, e.g. "0032974487"
  /// @param onDone  Called on completion (possibly on a background thread)
  ///               with the list of parsed licenses.  Empty list on failure.
  ///               Not called if the provider is being destroyed.
  void lookupFrn(const std::string &frn, ResultCb onDone);

private:
  // Shared with all detached lookup threads. alive_ is set to false and the
  // destructor waits for all in-flight threads to finish before returning,
  // preventing callbacks from firing after the provider is gone.
  std::shared_ptr<std::atomic<bool>> alive_ =
      std::make_shared<std::atomic<bool>>(true);
  std::mutex inflightMutex_;
  std::condition_variable inflightCv_;
  int inflight_ = 0;
};
