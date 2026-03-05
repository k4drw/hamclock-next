#include "CloudProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include "../ui/TextureManager.h"
#include <SDL_image.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

CloudProvider::CloudProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

CloudProvider::~CloudProvider() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (pendingSurface_) {
    SDL_FreeSurface(pendingSurface_);
    pendingSurface_ = nullptr;
  }
}

void CloudProvider::update() {
  // NASA GIBS WMS URL for VIIRS SNPP TrueColor.
  // VIIRS has a wider swath (~3000 km vs MODIS 2330 km) producing fewer
  // orbital gaps than MODIS Terra. We use "yesterday" to ensure the daily
  // composite is complete before requesting it.

  auto now = std::chrono::system_clock::now();
  auto yesterday = now - std::chrono::hours(24);
  std::time_t t = std::chrono::system_clock::to_time_t(yesterday);
  std::tm tm = *std::gmtime(&t);

  std::ostringstream oss;
  oss << "https://gibs.earthdata.nasa.gov/wms/epsg4326/best/wms.cgi?"
      << "SERVICE=WMS&REQUEST=GetMap&VERSION=1.3.0"
      << "&LAYERS=VIIRS_SNPP_CorrectedReflectance_TrueColor"
      << "&STYLES=&FORMAT=image/jpeg&CRS=CRS:84"
      << "&BBOX=-180,-90,180,90"
      << "&WIDTH=2048&HEIGHT=1024"
      << "&TIME=" << std::put_time(&tm, "%Y-%m-%d");

  std::string url = oss.str();
  LOG_I("CloudProvider", "Fetching clouds from {}", url);

  netMgr_.fetchAsync(
      url,
      [this, url](std::string data) {
        if (data.empty()) {
          LOG_E("CloudProvider", "Fetch failed or empty for {}", url);
          return;
        }

        // Offload decoding to background thread
        WorkerService::getInstance().submitTask([this, data = std::move(data)]() {
          SDL_Surface *surf = TextureManager::decodeToSurface(
              reinterpret_cast<const unsigned char *>(data.data()),
              static_cast<unsigned int>(data.size()), "clouds");

          if (surf) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pendingSurface_) {
              SDL_FreeSurface(pendingSurface_);
            }
            pendingSurface_ = surf;
            hasData_ = true;
            textureDirty_ = true;
            lastUpdateMs_ = SDL_GetTicks();
            LOG_I("CloudProvider", "Decoded {}x{} cloud surface in background",
                  surf->w, surf->h);
          } else {
            LOG_E("CloudProvider", "Failed to decode cloud JPEG in background");
          }
        });
      },
      3600); // Cache for 1 hour
}

bool CloudProvider::hasData() const { return hasData_; }

const std::string &CloudProvider::getData() const {
  static std::string empty;
  return empty; // jpgData_ removed to save RAM since we decode in background
}

SDL_Texture *CloudProvider::getTexture(SDL_Renderer *renderer, int w, int h) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check for new surface from background thread
  if (pendingSurface_) {
    if (texture_) {
      SDL_DestroyTexture(texture_);
    }
    texture_ = SDL_CreateTextureFromSurface(renderer, pendingSurface_);
    if (texture_) {
      texW_ = pendingSurface_->w;
      texH_ = pendingSurface_->h;
      SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(pendingSurface_);
    pendingSurface_ = nullptr;
    textureDirty_ = false;
  }

  if (!texture_) {
    return nullptr;
  }

  return texture_;
}
