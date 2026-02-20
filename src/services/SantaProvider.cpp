#include "SantaProvider.h"
#include <cmath>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void SantaProvider::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t now_t = std::chrono::system_clock::to_time_t(now);
  struct tm *tm = std::gmtime(&now_t);

  SantaData data;
  data.lastUpdate = now;

  // Active only in December
  if (tm->tm_mon == 11) {
    data.active = true;

    // Very simple logic: he goes around the world once every 24 hours on the
    // 24th. On other days, he is prepping at North Pole (90, 0)
    if (tm->tm_mday == 24) {
      double hrs = tm->tm_hour + tm->tm_min / 60.0 + tm->tm_sec / 3600.0;
      // Start at 180 (Date Line) and go West
      data.lon = 180.0 - (hrs / 24.0) * 360.0;
      if (data.lon < -180.0)
        data.lon += 360.0;

      // Simple sin wave for latitude
      data.lat = 45.0 * std::sin((hrs / 24.0) * 2.0 * M_PI * 10.0);
    } else {
      data.lat = 90.0;
      data.lon = 0.0;
    }
  } else {
    data.active = false;
    data.lat = 90.0;
    data.lon = 0.0;
  }

  store_->set(data);
}
