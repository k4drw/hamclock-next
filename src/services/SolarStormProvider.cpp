#include "SolarStormProvider.h"
#include "../core/Logger.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
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
  uint32_t now = SDL_GetTicks();
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
  netMgr_.fetchAsync(url, [this](std::string body) {
    if (!body.empty()) {
      processXrayFlux(body);
    }
  });
}

void SolarStormProvider::fetchAlerts() {
  const char *url = "https://services.swpc.noaa.gov/json/swpc_notifications_summary.json";
  netMgr_.fetchAsync(url, [this](std::string body) {
    if (!body.empty()) {
      processAlerts(body);
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

    if (callback_) callback_(data_);
  } catch (const std::exception &e) {
    LOG_E("SolarStormProvider", "Failed to parse X-ray JSON: {}", e.what());
  }
}

void SolarStormProvider::processAlerts(const std::string& body) {
  try {
    auto j = json::parse(body);
    std::lock_guard<std::mutex> lock(mutex_);

    // Simplified alert parsing logic: Look for CME predictions
    data_.cmeImpactPredicted = false;
    for (auto& alert : j) {
      std::string msg = alert.value("message", "");
      if (msg.find("CME") != std::string::npos && msg.find("IMPACT") != std::string::npos) {
        data_.cmeImpactPredicted = true;
        data_.alertMessage = "CME IMPACT PREDICTED";
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
