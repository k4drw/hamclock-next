#include "SolarStormProvider.h"
#include "../core/Astronomy.h"
#include "../core/Logger.h"
#include "../core/SoundManager.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace HamClock {

SolarStormProvider::SolarStormProvider(NetworkManager &netMgr) : netMgr_(netMgr) {
  std::lock_guard<std::mutex> lock(mutex_);
  std::fill_n(data_.fluxHistory, 60, 0.0f);
  lastFluxUpdate_ = 0;
  lastAlertUpdate_ = 0;
  LOG_I("SolarStormProvider", "SolarStormProvider initialized, waiting for update() call");
}

void SolarStormProvider::update() {
  lastFetchMs_ = SDL_GetTicks();
  uint32_t now = lastFetchMs_;
  // Use a special case for first update (lastUpdate == 0) to ensure immediate fetch
  if (lastFluxUpdate_ == 0 || (now - lastFluxUpdate_ >= FLUX_INTERVAL_MS)) {
    LOG_I("SolarStormProvider", "Triggering X-ray flux fetch");
    fetchXrayFlux();
    lastFluxUpdate_ = (now == 0 ? 1 : now); // Avoid zero to ensure logic works next frame
  }
  if (lastAlertUpdate_ == 0 || (now - lastAlertUpdate_ >= ALERT_INTERVAL_MS)) {
    LOG_I("SolarStormProvider", "Triggering alerts fetch");
    fetchAlerts();
    lastAlertUpdate_ = (now == 0 ? 1 : now);
  }
}

void SolarStormProvider::fetchXrayFlux() {
  const char *url = "https://services.swpc.noaa.gov/json/goes/primary/xrays-1-day.json";
  std::weak_ptr<SolarStormProvider> self = shared_from_this();
  netMgr_.fetchAsync(url, [self](std::string body) {
    auto p = self.lock();
    if (p && !body.empty()) {
      p->processXrayFlux(body);
    }
  });
}

void SolarStormProvider::fetchAlerts() {
  const char *url = "https://services.swpc.noaa.gov/products/alerts.json";
  std::weak_ptr<SolarStormProvider> self = shared_from_this();
  netMgr_.fetchAsync(url, [self](std::string body) {
    auto p = self.lock();
    if (p && !body.empty()) {
      p->processAlerts(body);
    }
  });
}

void SolarStormProvider::processXrayFlux(const std::string& body) {
  try {
    auto j = json::parse(body);
    if (!j.is_array() || j.empty()) {
      LOG_W("SolarStormProvider", "X-ray JSON is empty or not an array");
      return;
    }

    // Last 60 points are last hour
    std::lock_guard<std::mutex> lock(mutex_);
    
    float lastFlux = 0.0f;
    int pointsCollected = 0;
    
    // We want energy "0.1-0.8nm" (the 'long' channel)
    for (auto it = j.rbegin(); it != j.rend() && pointsCollected < 60; ++it) {
      if ((*it).contains("energy") && (*it)["energy"] == "0.1-0.8nm") {
        float flux = (*it).value("flux", 0.0f);
        if (pointsCollected == 0) {
            lastFlux = flux;
        }
        data_.fluxHistory[59 - pointsCollected] = flux;
        pointsCollected++;
      }
    }

    if (pointsCollected == 0) {
        LOG_W("SolarStormProvider", "No '0.1-0.8nm' flux points found in JSON");
        return;
    }

    data_.xrayFlux = lastFlux;
    data_.valid = true;
    LOG_I("SolarStormProvider", "Parsed {} flux points. Current: {:.2e}", pointsCollected, lastFlux);


    // R-scale Mapping (NOAA standard)
    if (lastFlux >= 2e-3) data_.rScale = SolarStormScale::Extreme; // R5
    else if (lastFlux >= 1e-3) data_.rScale = SolarStormScale::Severe; // R4
    else if (lastFlux >= 1e-4) data_.rScale = SolarStormScale::Strong; // R3
    else if (lastFlux >= 5e-5) data_.rScale = SolarStormScale::Moderate; // R2
    else if (lastFlux >= 1e-5) data_.rScale = SolarStormScale::Minor; // R1
    else data_.rScale = SolarStormScale::None;

    // Flare Class labeling
    if (lastFlux >= 1e-4) data_.lastFlareClass = "X";
    else if (lastFlux >= 1e-5) data_.lastFlareClass = "M";
    else if (lastFlux >= 1e-6) data_.lastFlareClass = "C";
    else if (lastFlux >= 1e-7) data_.lastFlareClass = "B";
    else data_.lastFlareClass = "A";

    // Voice alert on new X or M class flare (dedup: only once per class transition)
    if ((data_.lastFlareClass == "X" || data_.lastFlareClass == "M") &&
        data_.lastFlareClass != lastSpokenFlareClass_) {
      lastSpokenFlareClass_ = data_.lastFlareClass;
      SoundManager::getInstance().speak("Class " + data_.lastFlareClass + " solar flare");
    }
    if (data_.lastFlareClass != "X" && data_.lastFlareClass != "M") {
      lastSpokenFlareClass_.clear(); // reset when conditions improve
    }

    if (callback_) callback_(data_);
  } catch (const std::exception &e) {
    LOG_E("SolarStormProvider", "Failed to parse X-ray JSON: {}", e.what());
  }
}

// Parse NOAA "Valid From: YYYY Mon DD HHMM UTC" arrival time from a CME alert message.
static uint32_t parseCmeArrivalTime(const std::string &msg) {
  static const char *kMonths[12] = {
    "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
  };
  const char *searchFrom = msg.c_str();
  size_t vfPos = msg.find("Valid From:");
  if (vfPos != std::string::npos)
    searchFrom = msg.c_str() + vfPos + 11;
  int year = 0, day = 0, hhmm = 0;
  char monBuf[8] = {};
  if (sscanf(searchFrom, " %d %3s %d %4d UTC", &year, monBuf, &day, &hhmm) == 4) {
    int mon = -1;
    for (int i = 0; i < 12; ++i) {
      if (strncmp(monBuf, kMonths[i], 3) == 0) { mon = i; break; }
    }
    if (mon >= 0 && year >= 2020 && day >= 1 && day <= 31) {
      struct tm t = {};
      t.tm_year = year - 1900;
      t.tm_mon  = mon;
      t.tm_mday = day;
      t.tm_hour = hhmm / 100;
      t.tm_min  = hhmm % 100;
      t.tm_isdst = 0;
      time_t result = Astronomy::portable_timegm(&t);
      if (result > 0)
        return static_cast<uint32_t>(result);
    }
  }
  return 0;
}

void SolarStormProvider::processAlerts(const std::string& body) {
  try {
    auto j = json::parse(body);
    std::lock_guard<std::mutex> lock(mutex_);

    // alerts.json is an array of objects
    data_.cmeImpactPredicted = false;
    data_.cmeArrivalUtc = 0;
    for (auto& alert : j) {
      std::string message = alert.value("message", "");
      // Look for CME predictions in alerts
      if (message.find("CME") != std::string::npos &&
          (message.find("EXPECTED") != std::string::npos || message.find("PREDICTED") != std::string::npos)) {
        data_.cmeImpactPredicted = true;
        data_.alertMessage = "CME PREDICTED";
        data_.cmeArrivalUtc = parseCmeArrivalTime(message);
        break;
      }
    }
  } catch (const std::exception &e) {
    LOG_E("SolarStormProvider", "Failed to parse Alerts JSON: {}", e.what());
  }
}

SolarStormData SolarStormProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return data_;
}

} // namespace HamClock
