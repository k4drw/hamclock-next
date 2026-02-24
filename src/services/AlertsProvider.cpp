#include "AlertsProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

AlertsProvider::AlertsProvider(NetworkManager &net,
                               std::shared_ptr<AlertsStore> store)
    : net_(net), store_(std::move(store)) {}

void AlertsProvider::fetch(double lat, double lon, bool force) {
  char url[256];
  // NWS Alerts API: free, no key required.
  // Point format: lat,lon (comma-separated, not URL-encoded).
  std::snprintf(url, sizeof(url),
                "https://api.weather.gov/alerts/active?point=%.4f,%.4f",
                lat, lon);

  net_.fetchAsync(
      url,
      [](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([body]() {
          try {
            auto j = json::parse(body);
            auto *update = new AlertsData();
            if (j.contains("features") && j["features"].is_array()) {
              for (const auto &feature : j["features"]) {
                if (!feature.contains("properties"))
                  continue;
                const auto &props = feature["properties"];
                WeatherAlert alert;
                if (props.contains("id") && props["id"].is_string())
                  alert.id = props["id"];
                if (props.contains("event") && props["event"].is_string())
                  alert.event = props["event"];
                if (props.contains("severity") && props["severity"].is_string())
                  alert.severity = props["severity"];
                if (props.contains("headline") && props["headline"].is_string())
                  alert.headline = props["headline"];
                if (props.contains("expires") && props["expires"].is_string()) {
                  auto exp = props["expires"].get<std::string>();
                  alert.expires = exp.size() > 16 ? exp.substr(0, 16) : exp;
                }
                update->alerts.push_back(std::move(alert));
              }
            }
            update->valid = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_ALERTS_DATA_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      300, force);
}
