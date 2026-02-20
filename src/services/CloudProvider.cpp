#include "CloudProvider.h"
#include "../core/Logger.h"
#include <SDL.h>
#include <chrono>
#include <iomanip>
#include <sstream>

CloudProvider::CloudProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

void CloudProvider::update() {
  uint32_t now = SDL_GetTicks();
  // Update every 30 minutes (1800000 ms)
  if (hasData_ && (now - lastUpdateMs_ < 1800000)) {
    return;
  }

  // NASA GIBS Cloud Fraction (Terra/MODIS)
  // Example URL for a 2048x1024 equirectangular JPG
  // https://gibs.earthdata.nasa.gov/wmts/epsg4326/best/MODIS_Terra_Cloud_Fraction_Day/default/2024-02-20/2km/0/0/0.jpg
  // Actually, GIBS REST API is easier for single large images:
  // https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?SERVICE=WMS&REQUEST=GetMap&LAYERS=MODIS_Terra_Cloud_Fraction_Day&VERSION=1.3.0&FORMAT=image/jpeg&WIDTH=2048&HEIGHT=1024&CRS=EPSG:4326&BBOX=-90,-180,90,180

  auto now_time = std::chrono::system_clock::now();
  // GIBS data usually has a bit of lag, so use "today" but it might need yesterday if it's very early UTC
  std::time_t t = std::chrono::system_clock::to_time_t(now_time);
  std::tm tm = *std::gmtime(&t);
  
  std::stringstream ss;
  ss << "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?"
     << "SERVICE=WMS&REQUEST=GetMap&LAYERS=MODIS_Terra_Cloud_Fraction_Day&VERSION=1.3.0&FORMAT=image/jpeg"
     << "&WIDTH=2048&HEIGHT=1024&CRS=EPSG:4326&BBOX=-90,-180,90,180";

  std::string url = ss.str();

  LOG_I("CloudProvider", "Fetching global cloud overlay...");
  
  netMgr_.fetchAsync(url, [this, now](std::string data) {
    if (data.empty()) {
      LOG_E("CloudProvider", "Failed to fetch cloud imagery");
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    jpgData_ = std::move(data);
    hasData_ = true;
    lastUpdateMs_ = now;
    LOG_I("CloudProvider", "Global cloud imagery updated ({} bytes)", jpgData_.size());
  }, 1800); // 30 min cache
}

bool CloudProvider::hasData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return hasData_;
}

const std::string &CloudProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return jpgData_;
}
