#pragma once

#include "../core/LightningData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace HamClock {

class LightningProvider {
public:
  using Callback = std::function<void(const LightningData&)>;

  LightningProvider(NetworkManager& net);

  void fetch(double lat, double lon, bool force = false);
  void setCallback(Callback cb) { callback_ = std::move(cb); }

  LightningData getData() const;

private:
  void parseRainViewer(const std::string& timestampJson, double lat, double lon);
  void processStrikes(const std::string& strikesJson, double lat, double lon);

  NetworkManager& net_;
  Callback callback_;
  mutable std::mutex mutex_;
  LightningData data_;
  static constexpr uint32_t kFetchIntervalMs = 60'000; // 1 minute
  uint32_t lastFetch_ = 0;
  std::string lastTimestamp_;
};

} // namespace HamClock
