#include "CloudProvider.h"
#include "../core/Logger.h"
#include "../core/MemoryMonitor.h"
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
    MemoryMonitor::getInstance().destroyTexture(texture_);
  }
  if (pendingSurface_) {
    SDL_FreeSurface(pendingSurface_);
    pendingSurface_ = nullptr;
  }
}

void CloudProvider::update() {
  lastFetchMs_ = SDL_GetTicks();
  // NASA GIBS WMS URL for VIIRS SNPP TrueColor.
  // VIIRS has a wider swath (~3000 km vs MODIS 2330 km) producing fewer
  // orbital gaps than MODIS Terra. We use "yesterday" to ensure the daily
  // composite is complete before requesting it.

  auto now = std::chrono::system_clock::now();
  // VIIRS daily composites are typically complete by ~15:00 UTC the following day.
  // After that threshold use yesterday's data; before it fall back to 48h ago
  // so we never request an incomplete mosaic.
  std::time_t now_t = std::chrono::system_clock::to_time_t(now);
  std::tm now_utc = *std::gmtime(&now_t);
  int hoursBack = (now_utc.tm_hour >= 15) ? 24 : 48;
  auto yesterday = now - std::chrono::hours(hoursBack);
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
  hasData_ = false; // Mark as empty until new data arrives

  std::weak_ptr<CloudProvider> self = shared_from_this();
  netMgr_.fetchAsync(
      url,
      [self, url](std::string data) {
        auto p = self.lock();
        if (!p) return;
        if (data.empty()) {
          LOG_E("CloudProvider", "Fetch returned EMPTY data for {}", url);
          return;
        }
        LOG_I("CloudProvider", "Fetch received {} bytes of cloud data", data.size());

        // Offload decoding to background thread
        WorkerService::getInstance().submitTask([self, data = std::move(data)]() {
          auto p2 = self.lock();
          if (!p2) return;
          SDL_Surface *surf = TextureManager::decodeToSurface(
              reinterpret_cast<const unsigned char *>(data.data()),
              static_cast<unsigned int>(data.size()), "clouds");

          if (surf) {
            std::lock_guard<std::mutex> lock(p2->mutex_);
            if (p2->pendingSurface_) {
              SDL_FreeSurface(p2->pendingSurface_);
            }
            p2->pendingSurface_ = surf;
            p2->hasData_ = true;
            p2->textureDirty_ = true;
            p2->lastUpdateMs_ = SDL_GetTicks();
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

SDL_Texture *CloudProvider::getTexture(SDL_Renderer *renderer, int w, int h, SDL_Color tint) {
  std::lock_guard<std::mutex> lock(mutex_);

  // Check for new surface from background thread
  if (pendingSurface_) {
    if (texture_) {
      MemoryMonitor::getInstance().destroyTexture(texture_);
    }
    texture_ = SDL_CreateTextureFromSurface(renderer, pendingSurface_);
    if (texture_) {
      MemoryMonitor::getInstance().addVram((int64_t)pendingSurface_->w * pendingSurface_->h * 4);
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

  // Apply theme-based tinting
  SDL_SetTextureColorMod(texture_, tint.r, tint.g, tint.b);

  return texture_;
}

void CloudProvider::invalidateTexture() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (texture_) {
    MemoryMonitor::getInstance().destroyTexture(texture_);
  }
  if (pendingSurface_) {
    SDL_FreeSurface(pendingSurface_);
    pendingSurface_ = nullptr;
  }
}
