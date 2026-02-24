#include "ForecastProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ForecastProvider::ForecastProvider(NetworkManager &net,
                                   std::shared_ptr<ForecastStore> store)
    : net_(net), store_(std::move(store)) {}

void ForecastProvider::fetch(double lat, double lon, bool force) {
  // Step 1: Resolve the /points endpoint to get forecast URL for this lat/lon.
  char pointsUrl[256];
  std::snprintf(pointsUrl, sizeof(pointsUrl),
                "https://api.weather.gov/points/%.4f,%.4f", lat, lon);

  auto self = this;
  net_.fetchAsync(
      pointsUrl,
      [this](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([this, body]() {
          try {
            auto j = json::parse(body);
            if (j.contains("properties") &&
                j["properties"].contains("forecast") &&
                j["properties"]["forecast"].is_string()) {
              std::string forecastUrl =
                  j["properties"]["forecast"].get<std::string>();
              fetchForecast(forecastUrl, false);
            }
          } catch (...) {
          }
        });
      },
      86400); // Cache /points result for 24 h (it rarely changes)
  (void)self;
}

void ForecastProvider::fetchForecast(const std::string &forecastUrl,
                                     bool force) {
  net_.fetchAsync(
      forecastUrl,
      [](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([body]() {
          try {
            auto j = json::parse(body);
            auto *update = new ForecastData();

            if (j.contains("properties") &&
                j["properties"].contains("periods") &&
                j["properties"]["periods"].is_array()) {
              for (const auto &period : j["properties"]["periods"]) {
                ForecastPeriod fp;
                if (period.contains("name") && period["name"].is_string())
                  fp.name = period["name"];
                if (period.contains("temperature") &&
                    period["temperature"].is_number())
                  fp.tempF = period["temperature"].get<int>();
                if (period.contains("isDaytime") &&
                    period["isDaytime"].is_boolean())
                  fp.isDaytime = period["isDaytime"].get<bool>();
                if (period.contains("shortForecast") &&
                    period["shortForecast"].is_string())
                  fp.shortForecast = period["shortForecast"];
                if (period.contains("windSpeed") &&
                    period["windSpeed"].is_string())
                  fp.windSpeed = period["windSpeed"];
                if (period.contains("windDirection") &&
                    period["windDirection"].is_string())
                  fp.windDirection = period["windDirection"];
                if (period.contains("probabilityOfPrecipitation")) {
                  const auto &pop = period["probabilityOfPrecipitation"];
                  if (pop.contains("value") && pop["value"].is_number())
                    fp.precipitationPercent = pop["value"].get<int>();
                }
                update->periods.push_back(std::move(fp));
                if ((int)update->periods.size() >= 14)
                  break;
              }
            }

            update->valid = true;
            update->lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_FORECAST_DATA_READY;
            event.user.code = 0;
            event.user.data1 = update;
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      3600, force);
}
