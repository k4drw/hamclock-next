#include "TropoProvider.h"
#include "../core/Logger.h"
#include <SDL.h>
#include <cmath>
#include <nlohmann/json.hpp>

namespace HamClock {

TropoProvider::TropoProvider(NetworkManager& net) : net_(net) {}

TropoProvider::~TropoProvider() {
  alive_->store(false, std::memory_order_release);
}

void TropoProvider::fetch(double lat, double lon, bool force) {
  lastFetchMs_ = SDL_GetTicks();
  uint32_t now = SDL_GetTicks();
  if (!force && lastFetch_ > 0 && (now - lastFetch_) < kFetchIntervalMs) {
    return;
  }

  lastFetch_ = now;

  char url[256];
  std::snprintf(url, sizeof(url),
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&"
                "longitude=%.4f&hourly=temperature_2m,relative_humidity_2m,"
                "surface_pressure,wind_speed_10m&forecast_days=3",
                lat, lon);

  net_.fetchAsync(
      url,
      [this, alive = alive_](std::string response) {
        if (!alive->load(std::memory_order_acquire))
          return;
        if (response.empty()) {
          LOG_W("TropoProvider", "Empty response from Open-Meteo");
          return;
        }

        try {
          auto j = nlohmann::json::parse(response);
          auto &hourly = j["hourly"];
          auto &temps = hourly["temperature_2m"];
          auto &rh = hourly["relative_humidity_2m"];
          auto &press = hourly["surface_pressure"];
          auto &winds = hourly["wind_speed_10m"];

          TropoData newData;
          newData.valid = true;
          newData.location = "Open-Meteo";

          auto calcLevel = [](float t, float h, float p, float w) {
            float score = 0;
            // High Pressure is good for Tropo
            if (p > 1013) score += (p - 1013) * 0.2f;
            // Low Wind is necessary for stable layers
            if (w < 15) score += (15 - w) * 0.2f;
            // Small Dewpoint Spread indicates potential inversion
            float es = 6.112f * std::exp(17.67f * t / (t + 243.5f));
            float e = es * (h / 100.0f);
            float td = 243.5f * std::log(e / 6.112f) / (17.67f - std::log(e / 6.112f));
            float spread = t - td;
            if (spread < 5) score += (5 - spread) * 0.8f;

            int levelIdx = std::clamp(static_cast<int>(score), 0, 8);
            return static_cast<TropoLevel>(levelIdx);
          };

          // Use index 0 for current (approximate)
          newData.currentLevel = calcLevel(temps[0], rh[0], press[0], winds[0]);

          // Forecast every 6 hours
          for (int i = 6; i < static_cast<int>(temps.size()); i += 6) {
            TropoForecast f;
            f.hourOffset = i;
            f.level = calcLevel(temps[i], rh[i], press[i], winds[i]);
            f.description = tropoLevelToString(f.level);
            newData.forecast.push_back(f);
            if (newData.forecast.size() >= 6) break;
          }

          {
            std::lock_guard<std::mutex> lock(mutex_);
            data_ = newData;
          }

          if (callback_) {
            callback_(newData);
          }
        } catch (const std::exception &e) {
          LOG_E("TropoProvider", "JSON Parse error: {}", e.what());
        }
      },
      3600); // 1 hour network cache
}

TropoData TropoProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

} // namespace HamClock
