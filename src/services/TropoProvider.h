#pragma once

#include "ProviderBase.h"
#include "../core/TropoData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace HamClock {

class TropoProvider : public ::ProviderBase {
public:
  using Callback = std::function<void(const TropoData&)>;

  TropoProvider(NetworkManager& net);

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
  static constexpr uint32_t kFetchIntervalMs = 3'600'000; // 1 hour
  uint32_t lastFetch_ = 0;
};

} // namespace HamClock
