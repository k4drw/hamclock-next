#pragma once

#include "../core/MeteorData.h"
#include <functional>
#include <mutex>
#include <vector>

namespace HamClock {

class MeteorProvider {
public:
  using Callback = std::function<void(const MeteorData&)>;

  MeteorProvider();

  void update(double lat, double lon);
  void setCallback(Callback cb) { callback_ = std::move(cb); }

  MeteorData getData() const;

private:
  void calculateActivity(double lat, double lon);
  float getDiurnalMultiplier(int localHour);

  Callback callback_;
  mutable std::mutex mutex_;
  MeteorData data_;
  std::vector<MeteorShower> showers_;
  static constexpr uint32_t kUpdateIntervalMs = 600'000; // 10 minutes
  uint32_t lastUpdate_ = 0;
};

} // namespace HamClock
