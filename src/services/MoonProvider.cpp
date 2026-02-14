#include "MoonProvider.h"
#include "../core/Astronomy.h"
#include "../core/Logger.h"
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using json = nlohmann::json;

MoonProvider::MoonProvider(NetworkManager &net,
                           std::shared_ptr<MoonStore> store)
    : net_(net), store_(std::move(store)) {}

void MoonProvider::update(double lat, double lon) {
  (void)lat;
  (void)lon;

  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&now_c, &utc);

  // NASA Dial-a-Moon API prefers YYYY-MM-DDTHH:00
  std::stringstream ss;
  ss << std::put_time(&utc, "%Y-%m-%dT%H:00");
  std::string isoDate = ss.str();

  std::string url = "https://svs.gsfc.nasa.gov/api/dialamoon/" + isoDate;

  auto store = store_;
  net_.fetchAsync(url, [isoDate, store](std::string body) {
    if (body.empty()) {
      LOG_E("MoonProvider", "Failed to fetch NASA data for {}", isoDate);
      return;
    }

    try {
      auto j = json::parse(body);
      MoonData data;

      // Dial-a-Moon phase is percentage (0-100), convert to 0-1 (normalized)
      // Note: Dial-a-Moon 'phase' is illumination percentage.
      data.illumination = j.value("phase", 0.0);
      double age = j.value("age", 0.0); // Days since new moon
      double cycle = 29.53;
      data.phase = age / cycle;

      data.imageUrl = j["image"]["url"].get<std::string>();
      data.posangle = j.value("posangle", 0.0);

      // Phase naming
      if (data.illumination < 2.0)
        data.phaseName = "New";
      else if (data.illumination > 98.0)
        data.phaseName = "Full";
      else {
        bool waxing = age < (cycle / 2.0);
        if (data.illumination < 45.0)
          data.phaseName = waxing ? "Waxing Cres" : "Waning Cres";
        else if (data.illumination < 55.0)
          data.phaseName = waxing ? "First Qtr" : "Third Qtr";
        else
          data.phaseName = waxing ? "Waxing Gib" : "Waning Gib";
      }

      data.azimuth = 0.0;
      data.elevation = 0.0;
      data.valid = true;
      store->update(data);

      LOG_I("MoonProvider", "Updated for {} ({:.1f}% illum, {})", isoDate,
            data.illumination, data.phaseName);

    } catch (const std::exception &e) {
      LOG_E("MoonProvider", "JSON error: {}", e.what());
    }
  });
}
