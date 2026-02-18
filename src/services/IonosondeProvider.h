#pragma once

#include "../core/IonosondeData.h"
#include "../network/NetworkManager.h"
#include <mutex>
#include <vector>

class IonosondeProvider {
public:
  IonosondeProvider(NetworkManager &netMgr);

  /**
   * Trigger asynchronous update from KC2G API.
   * Throttled to 10 minutes.
   */
  void update();

  /**
   * Interpolate ionospheric parameters at a given location.
   * Returns an InterpolatedIonosonde struct.
   */
  InterpolatedIonosonde interpolate(double lat, double lon) const;

  bool hasData() const;
  uint32_t getLastUpdateMs() const { return lastUpdateMs_; }

private:
  void processData(const std::string &body);

  NetworkManager &netMgr_;
  std::vector<IonosondeStation> stations_;
  bool hasData_ = false;
  uint32_t lastUpdateMs_ = 0;
  mutable std::mutex mutex_;

  static constexpr uint32_t UPDATE_INTERVAL_MS = 600000; // 10 minutes
  static constexpr double MAX_VALID_DISTANCE_KM = 3000.0;
};
