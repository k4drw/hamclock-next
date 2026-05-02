#include "OpenMeteoForecastProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

static std::string weatherCodeToDescription(int code) {
  switch (code) {
  case 0:
    return "Clear sky";
  case 1:
  case 2:
  case 3:
    return "Partly cloudy";
  case 45:
  case 48:
    return "Fog";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 56:
  case 57:
    return "Freezing Drizzle";
  case 61:
  case 63:
  case 65:
    return "Rain";
  case 66:
  case 67:
    return "Freezing Rain";
  case 71:
  case 73:
  case 75:
    return "Snow fall";
  case 77:
    return "Snow grains";
  case 80:
  case 81:
  case 82:
    return "Rain showers";
  case 85:
  case 86:
    return "Snow showers";
  case 95:
    return "Thunderstorm";
  case 96:
  case 99:
    return "Thunderstorm with hail";
  default:
    return "Unknown";
  }
}

static std::string dateToWeekday(const std::string &dateStr, int dayIndex) {
  if (dayIndex == 0) {
    return "Today";
  }
  struct tm tm_info = {};
  std::istringstream ss(dateStr);
  ss >> std::get_time(&tm_info, "%Y-%m-%d");
  if (ss.fail())
    return "Day" + std::to_string(dayIndex);
  char buf[16];
  std::strftime(buf, sizeof(buf), "%A", &tm_info);
  return buf;
}

OpenMeteoForecastProvider::OpenMeteoForecastProvider(
    NetworkManager &net, std::shared_ptr<ForecastStore> store)
    : net_(net), store_(std::move(store)) {}

void OpenMeteoForecastProvider::fetch(double lat, double lon, bool force) {
  lastFetchMs_ = SDL_GetTicks();
  char url[512];
  std::snprintf(url, sizeof(url),
                "https://api.open-meteo.com/v1/forecast?"
                "latitude=%.4f&longitude=%.4f"
                "&daily=weathercode,temperature_2m_max,temperature_2m_min,"
                "precipitation_probability_max"
                "&temperature_unit=fahrenheit&timezone=auto&forecast_days=7",
                lat, lon);

  std::weak_ptr<OpenMeteoForecastProvider> self = shared_from_this();
  net_.fetchAsync(
      url,
      [self](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([self, body]() {
          auto p = self.lock();
          if (!p)
            return;

          try {
            auto j = json::parse(body);
            if (!j.contains("daily"))
              return;

            auto daily = j["daily"];
            if (!daily.contains("time") || !daily["time"].is_array() ||
                !daily.contains("weathercode") ||
                !daily["weathercode"].is_array() ||
                !daily.contains("temperature_2m_max") ||
                !daily["temperature_2m_max"].is_array() ||
                !daily.contains("temperature_2m_min") ||
                !daily["temperature_2m_min"].is_array() ||
                !daily.contains("precipitation_probability_max") ||
                !daily["precipitation_probability_max"].is_array())
              return;

            auto *update = new ForecastData();
            const auto &times = daily["time"];
            const auto &codes = daily["weathercode"];
            const auto &tempMaxes = daily["temperature_2m_max"];
            const auto &tempMins = daily["temperature_2m_min"];
            const auto &precips = daily["precipitation_probability_max"];

            for (size_t i = 0; i < times.size() && i < 7; ++i) {
              // Daytime period
              ForecastPeriod daytime;
              daytime.name = dateToWeekday(times[i].get<std::string>(), i);
              daytime.tempF = tempMaxes[i].is_number()
                                  ? (int)tempMaxes[i].get<double>()
                                  : 0;
              daytime.isDaytime = true;
              daytime.shortForecast = weatherCodeToDescription(
                  codes[i].is_number() ? codes[i].get<int>() : 0);
              daytime.precipitationPercent = precips[i].is_number()
                                                 ? (int)precips[i].get<double>()
                                                 : 0;
              update->periods.push_back(std::move(daytime));

              // Nighttime period
              ForecastPeriod nighttime;
              nighttime.name = daytime.name + " Night";
              nighttime.tempF = tempMins[i].is_number()
                                    ? (int)tempMins[i].get<double>()
                                    : 0;
              nighttime.isDaytime = false;
              nighttime.shortForecast = weatherCodeToDescription(
                  codes[i].is_number() ? codes[i].get<int>() : 0);
              nighttime.precipitationPercent = 0;
              update->periods.push_back(std::move(nighttime));
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
      14400, force);
}
