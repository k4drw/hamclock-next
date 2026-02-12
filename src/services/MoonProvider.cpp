#include "MoonProvider.h"
#include <cmath>
#include <ctime>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MoonProvider::MoonProvider(std::shared_ptr<MoonStore> store)
    : store_(std::move(store)) {}

void MoonProvider::update(double lat, double lon) {
  (void)lat;
  (void)lon;

  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);

  // Base date: New Moon on Jan 6, 2000, 18:14 UTC
  struct tm base_tm = {};
  base_tm.tm_year = 100; // 2000
  base_tm.tm_mon = 0;    // Jan
  base_tm.tm_mday = 6;
  base_tm.tm_hour = 18;
  base_tm.tm_min = 14;
  base_tm.tm_isdst = 0;

  // timegm is a non-standard but widely available POSIX function (including
  // Linux) For Windows, _mkgmtime could be used, but this project target is
  // Linux.
  std::time_t base_c = timegm(&base_tm);

  double lunarCycle = 29.530588853;
  double diffSecs = difftime(now_c, base_c);
  double ageDays = std::fmod(diffSecs / 86400.0, lunarCycle);
  if (ageDays < 0)
    ageDays += lunarCycle;

  MoonData data;
  data.phase = ageDays / lunarCycle;
  // Illumination: 0 at New (0.0), 100 at Full (0.5)
  data.illumination = 100.0 * (0.5 * (1.0 - std::cos(2.0 * M_PI * data.phase)));

  if (data.phase < 0.03 || data.phase > 0.97)
    data.phaseName = "New";
  else if (data.phase < 0.22)
    data.phaseName = "Waxing Cres";
  else if (data.phase < 0.28)
    data.phaseName = "First Qtr";
  else if (data.phase < 0.47)
    data.phaseName = "Waxing Gib";
  else if (data.phase < 0.53)
    data.phaseName = "Full";
  else if (data.phase < 0.72)
    data.phaseName = "Waning Gib";
  else if (data.phase < 0.78)
    data.phaseName = "Third Qtr";
  else
    data.phaseName = "Waning Cres";

  // Placeholder Az/El
  data.azimuth = 0.0;
  data.elevation = 0.0;

  data.valid = true;
  store_->update(data);
}
