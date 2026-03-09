#include "LightningProvider.h"
#include "../core/Logger.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

namespace HamClock {

LightningProvider::LightningProvider(NetworkManager &net) : net_(net) {}

void LightningProvider::fetch(double lat, double lon, bool force) {
  uint32_t nowMs = SDL_GetTicks();
  if (!force && lastFetch_ > 0 && (nowMs - lastFetch_) < kFetchIntervalMs) {
    return;
  }

  lastFetch_ = nowMs;
  LOG_I("LightningProvider", "Fetching lightning data (4-tile scan)...");

  // 1. Fetch RainViewer for specific strike clusters (Primary)
  std::weak_ptr<LightningProvider> self = shared_from_this();
  net_.fetchAsync(
      "https://api.rainviewer.com/public/weather-maps.json",
      [self, lat, lon](std::string response) {
        auto p = self.lock();
        if (!p)
          return;
        if (response.empty()) {
          LOG_W("LightningProvider", "Metadata response empty");
          return;
        }
        try {
          auto j = nlohmann::json::parse(response);
          if (j.contains("lightning") && !j["lightning"].empty()) {
            auto latest = j["lightning"].back();
            int ts = latest["time"].get<int>();
            std::string tsStr = std::to_string(ts);

            if (tsStr != p->lastTimestamp_) {
              p->lastTimestamp_ = tsStr;
              LOG_I("LightningProvider", "Found new lightning data (ts: {})", ts);
              
              int cx = static_cast<int>((lon + 180.0) / 360.0 * 32.0);
              int cy = static_cast<int>((1.0 - std::log(std::tan(lat * M_PI / 180.0) + 1.0 / std::cos(lat * M_PI / 180.0)) / M_PI) / 2.0 * 32.0);

              for (int dx = -1; dx <= 0; ++dx) {
                for (int dy = -1; dy <= 0; ++dy) {
                  char url[256];
                  std::snprintf(url, sizeof(url), "https://nowcast.rainviewer.com/v2/lightning/%d/256/5/%d/%d/1/1_1.json", ts, cx+dx, cy+dy);
                  LOG_D("LightningProvider", "Requesting tile: {}", url);
                  // Timestamped tiles are immutable, so 1 hour cache is safe
                  p->net_.fetchAsync(url, [self, lat, lon](std::string strikeData) {
                    auto p2 = self.lock();
                    if (p2) {
                      LOG_D("LightningProvider", "Received tile ({} bytes)", strikeData.size());
                      p2->processStrikes(strikeData, lat, lon);
                    }
                  }, 3600);
                }
              }
            }
          } else {
            LOG_I("LightningProvider", "No lightning activity found in RainViewer feed");
            std::lock_guard<std::mutex> lock(p->mutex_);
            p->data_.valid = true;
            if (p->callback_) p->callback_(p->data_);
          }
        } catch (const std::exception &e) {
          LOG_E("LightningProvider", "Metadata parse error: {}", e.what());
        }
      }, 60); // Metadata refreshed every 60 seconds

  // Secondary broad check: Open-Meteo
  char meteoUrl[256];
  std::snprintf(meteoUrl, sizeof(meteoUrl), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=weather_code", lat, lon);
  net_.fetchAsync(meteoUrl, [self](std::string response) {
    auto p = self.lock();
    if (!p)
      return;
    if (response.empty()) {
        LOG_W("LightningProvider", "Open-Meteo response empty");
        return;
    }
    try {
      auto j = nlohmann::json::parse(response);
      int code = j["current"]["weather_code"].get<int>();
      LOG_I("LightningProvider", "Open-Meteo weather code: {}", code);
      std::lock_guard<std::mutex> lock(p->mutex_);
      bool stormy = (code == 95 || code == 96 || code == 99);
      if (stormy && !p->data_.safetyAlert) {
        LOG_I("LightningProvider", "Broad thunderstorm alert active based on WMO code {}", code);
        p->data_.safetyAlert = true;
      }
      p->data_.valid = true;
      if (p->callback_) p->callback_(p->data_);
    } catch (const std::exception &e) {
        LOG_E("LightningProvider", "Open-Meteo parse error: {}", e.what());
    }
  }, 300); // 5 min cache for broad weather code
}

void LightningProvider::processStrikes(const std::string &strikesJson,
                                       double deLat, double deLon) {
  try {
    if (strikesJson.empty()) return;
    auto j = nlohmann::json::parse(strikesJson);
    auto now = std::chrono::system_clock::now();
    int strikesInJson = 0;
    int strikesInArea = 0;

    std::lock_guard<std::mutex> lock(mutex_);

    if (j.is_array()) {
      strikesInJson = static_cast<int>(j.size());
      for (const auto &s : j) {
        if (s.size() < 3) continue;
        double sLat = s[1].get<double>();
        double sLon = s[2].get<double>();

        double dy = (sLat - deLat) * M_PI / 180.0;
        double dx = (sLon - deLon) * M_PI / 180.0 * std::cos(deLat * M_PI / 180.0);
        double dist = std::sqrt(dx * dx + dy * dy) * 6371.0;
        double bearing = std::atan2(dx, dy) * 180.0 / M_PI;
        if (bearing < 0) bearing += 360.0;

        if (dist < 250.0) {
          strikesInArea++;
          LightningStrike strike;
          strike.lat = sLat;
          strike.lon = sLon;
          strike.distanceKm = dist;
          strike.bearing = bearing;
          strike.time = now;
          data_.recentStrikes.push_back(strike);
        }
      }
    }

    LOG_D("LightningProvider", "Processed {} strikes from JSON, found {} within 250km area", strikesInJson, strikesInArea);

    auto pruneTime = now - std::chrono::minutes(30);
    data_.recentStrikes.erase(
        std::remove_if(data_.recentStrikes.begin(), data_.recentStrikes.end(),
                       [pruneTime](const LightningStrike &s) {
                         return s.time < pruneTime;
                       }),
        data_.recentStrikes.end());

    int count10 = 0;
    int count30 = 0;
    double closest = 9999.0;
    double closestBrg = 0.0;
    auto tenMinAgo = now - std::chrono::minutes(10);

    for (const auto &s : data_.recentStrikes) {
      count30++;
      if (s.time > tenMinAgo) count10++;
      if (s.distanceKm < closest) {
        closest = s.distanceKm;
        closestBrg = s.bearing;
      }
    }

    data_.strikesLast10Min = count10;
    data_.strikesLast30Min = count30;
    data_.closestStrikeKm = (closest > 9000.0) ? -1.0 : closest;
    data_.closestBearing = closestBrg;
    data_.safetyAlert = (data_.closestStrikeKm > 0 && data_.closestStrikeKm < 15.0);
    data_.valid = true;

    if (callback_) callback_(data_);
  } catch (...) {}
}

LightningData LightningProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

} // namespace HamClock
