#pragma once

#include "../core/SolarStormData.h"
#include "../network/NetworkManager.h"
#include <functional>
#include <mutex>
#include <string>

namespace HamClock {

class SolarStormProvider {
public:
  using Callback = std::function<void(const SolarStormData&)>;

  SolarStormProvider(NetworkManager &netMgr);

  void update();
  void setCallback(Callback cb) { callback_ = std::move(cb); }
  SolarStormData getData() const;

private:
  void fetchXrayFlux();
  void fetchAlerts();
  
  void processXrayFlux(const std::string& body);
  void processAlerts(const std::string& body);

  NetworkManager &netMgr_;
  SolarStormData data_;
  mutable std::mutex mutex_;
  Callback callback_;

  uint32_t lastFluxUpdate_ = 0;
  uint32_t lastAlertUpdate_ = 0;

  static constexpr uint32_t FLUX_INTERVAL_MS = 60000; // 1 min (high frequency)
  static constexpr uint32_t ALERT_INTERVAL_MS = 300000; // 5 mins
};

} // namespace HamClock
