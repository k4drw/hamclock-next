#include "WinlinkProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kEarthR = 6371.0;

WinlinkProvider::WinlinkProvider(NetworkManager &net,
                                 std::shared_ptr<WinlinkStore> store)
    : net_(net), store_(std::move(store)) {}

double WinlinkProvider::haversineKm(double lat1, double lon1, double lat2,
                                    double lon2) {
  auto toRad = [](double d) { return d * kPi / 180.0; };
  double dLat = toRad(lat2 - lat1);
  double dLon = toRad(lon2 - lon1);
  double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
             std::cos(toRad(lat1)) * std::cos(toRad(lat2)) *
                 std::sin(dLon / 2) * std::sin(dLon / 2);
  return 2.0 * kEarthR * std::asin(std::sqrt(a));
}

void WinlinkProvider::fetch(double lat, double lon, int radiusKm, bool force) {
  // Winlink API gateway listing endpoint.
  auto &cfg = ConfigManager::instance().getConfig();
  std::string key = cfg.winlinkKey;

  char url[512];
  std::snprintf(url, sizeof(url),
                "https://api.winlink.org/json/metadata?op=GatewayListing%s%s",
                key.empty() ? "" : "&key=", key.c_str());

  (void)radiusKm; // filtering done client-side after fetch
  const char *resolvedUrl = url;

  double deLat = lat;
  double deLon = lon;

  net_.fetchAsync(
      resolvedUrl,
      [deLat, deLon, radiusKm](std::string body) {
        if (body.empty()) {
          // HTTP error — signal "fetched but unavailable" so panel shows a useful message.
          auto *update = new WinlinkData();
          update->fetched = true;
          update->valid = false;
          SDL_Event ev;
          SDL_zero(ev);
          ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_WINLINK_DATA_READY;
          ev.user.code = 0;
          ev.user.data1 = update;
          SDL_PushEvent(&ev);
          return;
        }

        WorkerService::getInstance().submitTask([body, deLat, deLon, radiusKm]() {
          try {
            auto j = json::parse(body);
            auto *update = new WinlinkData();

            // Winlink returns {"Gateways": [...]}
            const json *gws = nullptr;
            if (j.contains("Gateways") && j["Gateways"].is_array())
              gws = &j["Gateways"];

            if (gws) {
              for (const auto &g : *gws) {
                WinlinkGateway gw;
                auto str = [&](const char *k) -> std::string {
                  return (g.contains(k) && g[k].is_string())
                             ? g[k].get<std::string>()
                             : "";
                };
                gw.callsign = str("Callsign");
                if (gw.callsign.empty())
                  continue;

                if (g.contains("Latitude") && g["Latitude"].is_number())
                  gw.lat = g["Latitude"].get<double>();
                if (g.contains("Longitude") && g["Longitude"].is_number())
                  gw.lon = g["Longitude"].get<double>();
                if (g.contains("Frequency") && g["Frequency"].is_number())
                  gw.freqMHz = g["Frequency"].get<double>() / 1e6;
                gw.modes = str("Modes");
                auto ts = str("LastHeard");
                gw.lastHeard = ts.size() > 16 ? ts.substr(0, 16) : ts;

                if (gw.lat != 0.0 && gw.lon != 0.0)
                  gw.distanceKm =
                      haversineKm(deLat, deLon, gw.lat, gw.lon);

                // Client-side radius filter
                if (gw.distanceKm <= radiusKm)
                  update->gateways.push_back(std::move(gw));
              }
            }

            std::sort(update->gateways.begin(), update->gateways.end(),
                      [](const WinlinkGateway &a, const WinlinkGateway &b) {
                        return a.distanceKm < b.distanceKm;
                      });
            if (update->gateways.size() > 50)
              update->gateways.resize(50);

            update->valid = true;
            update->fetched = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_WINLINK_DATA_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      3600, force);
}
