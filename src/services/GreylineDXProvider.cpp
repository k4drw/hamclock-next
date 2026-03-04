#include "GreylineDXProvider.h"
#include "../core/Astronomy.h"
#include "../core/DXCCData.h"
#include <algorithm>

GreylineDXProvider::GreylineDXProvider(PrefixManager &pm,
                                       std::shared_ptr<GreylineDXStore> store)
    : pm_(pm), store_(std::move(store)) {}

void GreylineDXProvider::update() {
  GreylineDXData update;
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  double currentUtcHours =
      utc.tm_hour + utc.tm_min / 60.0 + utc.tm_sec / 3600.0;
  int doy = utc.tm_yday + 1;

  for (size_t i = 0; i < DXCC_ENTITIES_COUNT; ++i) {
    const auto &entity = DXCC_ENTITIES[i];
    SunTimes times = Astronomy::calculateSunTimes(entity.lat, entity.lon, doy);

    if (!times.hasRise && !times.hasSet)
      continue; // Polar day/night

    auto checkWindow = [&](double targetTime, bool isRise) {
      double diff = targetTime - currentUtcHours;
      // Handle wrap around
      if (diff > 12.0)
        diff -= 24.0;
      else if (diff < -12.0)
        diff += 24.0;

      double diffMins = diff * 60.0;

      // Window is +/- 15 minutes
      if (std::abs(diffMins) <= 15.0) {
        GreylineEntity ge;
        ge.name = entity.name;
        ge.prefix = pm_.getPrefix(entity.dxcc);
        ge.lat = entity.lat;
        ge.lon = entity.lon;
        ge.minutesToPeak = diffMins;
        ge.isSunrise = isRise;
        update.activeEntities.push_back(ge);
      }
    };

    if (times.hasRise)
      checkWindow(times.sunrise, true);
    if (times.hasSet)
      checkWindow(times.sunset, false);
  }

  // Sort by absolute time to peak (closest first)
  std::sort(update.activeEntities.begin(), update.activeEntities.end(),
            [](const GreylineEntity &a, const GreylineEntity &b) {
              return std::abs(a.minutesToPeak) < std::abs(b.minutesToPeak);
            });

  update.valid = true;
  update.lastUpdate = now;
  store_->update(update);
}
