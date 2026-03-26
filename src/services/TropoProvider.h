#pragma once

#include "ProviderBase.h"
#include "../core/TropoData.h"
#include "../network/NetworkManager.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace HamClock {

class TropoProvider : public ::ProviderBase {
public:
  using Callback = std::function<void(const TropoData&)>;

  TropoProvider(NetworkManager& net);
  ~TropoProvider();

  void fetch(double lat, double lon, bool force = false);
  void setCallback(Callback cb) { callback_ = std::move(cb); }

  TropoData getData() const;

private:
  void parseHepburn(const std::string& html);
  void parseAPRS(const std::string& data);

  NetworkManager& net_;
  Callback callback_;
  mutable std::mutex mutex_;
  TropoData data_;
  // Shared with the fetchAsync callback. Set to false in the destructor so
  // detached network threads that outlive this object skip writing to freed
  // members (mirrors NetworkManager's own alive_ guard pattern).
  std::shared_ptr<std::atomic<bool>> alive_ =
      std::make_shared<std::atomic<bool>>(true);
  static constexpr uint32_t kFetchIntervalMs = 3'600'000; // 1 hour
  uint32_t lastFetch_ = 0;
};

} // namespace HamClock
