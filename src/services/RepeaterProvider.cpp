#include "RepeaterProvider.h"
#include "../core/Constants.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kEarthR = 6371.0;

RepeaterProvider::RepeaterProvider(NetworkManager &net,
                                   std::shared_ptr<RepeaterStore> store)
    : net_(net), store_(std::move(store)) {}

double RepeaterProvider::haversineKm(double lat1, double lon1, double lat2,
                                     double lon2) {
  auto toRad = [](double d) { return d * kPi / 180.0; };
  double dLat = toRad(lat2 - lat1);
  double dLon = toRad(lon2 - lon1);
  double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
             std::cos(toRad(lat1)) * std::cos(toRad(lat2)) *
                 std::sin(dLon / 2) * std::sin(dLon / 2);
  return 2.0 * kEarthR * std::asin(std::sqrt(a));
}

void RepeaterProvider::fetch(double lat, double lon, const std::string &state,
                              bool force) {
  // RepeaterBook API. Use key if configured in AppConfig.
  auto &cfg = ConfigManager::instance().getConfig();
  std::string key = cfg.repeaterBookKey;

  char url[512];
  if (!state.empty()) {
    std::snprintf(
        url, sizeof(url),
        "https://www.repeaterbook.com/api/export.php?country=United%%20States"
        "&state=%s&lat=%.4f&lng=%.4f&dist=93&Dunit=k&fmt=json%s%s",
        state.c_str(), lat, lon, key.empty() ? "" : "&key=", key.c_str());
  } else {
    std::snprintf(
        url, sizeof(url),
        "https://www.repeaterbook.com/api/export.php?country=United%%20States"
        "&lat=%.4f&lng=%.4f&dist=93&Dunit=k&fmt=json%s%s",
        lat, lon, key.empty() ? "" : "&key=", key.c_str());
  }

  double deLat = lat;
  double deLon = lon;

  net_.fetchAsync(
      url,
      [deLat, deLon](std::string body) {
        if (body.empty()) {
          // HTTP error (e.g. 401 Unauthorized — RepeaterBook requires API key).
          // Signal "fetched but unavailable" so the panel shows a useful message.
          auto *update = new RepeaterData();
          update->fetched = true;
          update->valid = false;
          SDL_Event ev;
          SDL_zero(ev);
          ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_REPEATER_DATA_READY;
          ev.user.code = 0;
          ev.user.data1 = update;
          SDL_PushEvent(&ev);
          return;
        }

        WorkerService::getInstance().submitTask([body, deLat, deLon]() {
          try {
            auto j = json::parse(body);
            auto *update = new RepeaterData();

            // RepeaterBook returns {"count":N,"results":[...]}
            const json *results = nullptr;
            if (j.contains("results") && j["results"].is_array())
              results = &j["results"];
            else if (j.is_array())
              results = &j;

            if (results) {
              for (const auto &r : *results) {
                RepeaterEntry re;
                auto strVal = [&](const char *key) -> std::string {
                  if (r.contains(key) && r[key].is_string())
                    return r[key].get<std::string>();
                  return {};
                };
                re.callsign = strVal("Callsign");
                std::string freqStr = strVal("Frequency");
                if (!freqStr.empty())
                  re.freqMHz = StringUtils::safe_stod(freqStr);
                std::string offStr = strVal("Offset");
                if (!offStr.empty())
                  re.offsetMHz = StringUtils::safe_stod(offStr);
                re.tone = strVal("PL");
                re.mode = strVal("Use"); // Note: RepeaterBook "Use" = mode type
                re.use = strVal("Operational Status");
                re.state = strVal("State");
                re.county = strVal("County");

                double rLat = 0, rLon = 0;
                std::string latStr = strVal("Lat");
                std::string lonStr = strVal("Long");
                if (!latStr.empty())
                  rLat = StringUtils::safe_stod(latStr);
                if (!lonStr.empty())
                  rLon = StringUtils::safe_stod(lonStr);
                if (rLat != 0 && rLon != 0)
                  re.distanceKm =
                      haversineKm(deLat, deLon, rLat, rLon);

                if (!re.callsign.empty() && re.freqMHz > 0)
                  update->repeaters.push_back(std::move(re));
              }
            }

            // Sort by distance
            std::sort(update->repeaters.begin(), update->repeaters.end(),
                      [](const RepeaterEntry &a, const RepeaterEntry &b) {
                        return a.distanceKm < b.distanceKm;
                      });

            update->valid = true;
            update->fetched = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_REPEATER_DATA_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      86400, force);
}
