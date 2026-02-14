#include "DstProvider.h"
#include "../core/Astronomy.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DstProvider::DstProvider(NetworkManager &net, std::shared_ptr<DstStore> store)
    : net_(net), store_(store) {}

void DstProvider::fetch() {
  const char *url = "https://services.swpc.noaa.gov/products/kyoto-dst.json";

  net_.fetchAsync(url, [this](std::string body) {
    if (body.empty())
      return;

    try {
      auto j = json::parse(body);
      if (!j.is_array())
        return;

      DstData data;
      auto now = std::chrono::system_clock::now();
      time_t now_t = std::chrono::system_clock::to_time_t(now);

      // Format: [ ["2024-03-01 00:00:00", -10], ... ]
      // Skip header (index 0)
      for (size_t i = 1; i < j.size(); ++i) {
        auto row = j[i];
        if (row.size() < 2)
          continue;

        std::string time_str = row[0];
        float val = 0;
        if (row[1].is_string())
          val = std::stof(row[1].get<std::string>());
        else if (row[1].is_number())
          val = row[1].get<float>();

        // Parse YYYY-MM-DD HH:MM:SS
        struct tm t = {0};
        int y, mon, day, hr, min, sec;
        if (std::sscanf(time_str.c_str(), "%d-%d-%d %d:%d:%d", &y, &mon, &day,
                        &hr, &min, &sec) == 6) {
          t.tm_year = y - 1900;
          t.tm_mon = mon - 1;
          t.tm_mday = day;
          t.tm_hour = hr;
          t.tm_min = min;
          t.tm_sec = sec;
          time_t ts = Astronomy::portable_timegm(&t);
          float age_hrs = (ts - now_t) / 3600.0f;

          // Keep last 48 hours
          if (age_hrs > -48.0f) {
            data.points.push_back({age_hrs, val});
          }
        }
      }

      if (!data.points.empty()) {
        std::sort(data.points.begin(), data.points.end(),
                  [](const DstPoint &a, const DstPoint &b) {
                    return a.age_hrs < b.age_hrs;
                  });
        data.current_val = data.points.back().value;
        data.valid = true;
        store_->set(data);
      }
    } catch (...) {
    }
  });
}
