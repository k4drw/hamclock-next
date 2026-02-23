#include "CloudProvider.h"
#include "../core/Logger.h"
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

        // Validate JPEG header (magic number)
        if (data.size() < 4 || (uint8_t)data[0] != 0xFF ||
            (uint8_t)data[1] != 0xD8) {
          LOG_E("CloudProvider", "Invalid JPEG data received from {}", url);
          return;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        jpgData_ = std::move(data);
        hasData_ = true;
        textureDirty_ = true;
        lastUpdateMs_ = SDL_GetTicks();
        LOG_I("CloudProvider", "Received {} bytes of cloud data",
              jpgData_.size());
      },
      3600); // Cache for 1 hour
}

bool CloudProvider::hasData() const { return hasData_; }

const std::string &CloudProvider::getData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return jpgData_;
}

SDL_Texture *CloudProvider::getTexture(SDL_Renderer *renderer, int w, int h) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!hasData_ || jpgData_.empty()) {
    return nullptr;
  }

  // If we have a texture and it's not dirty, return it.
  // Note: we don't strictly enforce w/h match here because we might want to stretch it.
  // But if we wanted to support resizing efficiently, we might re-create.
  // For now, we just rely on SDL_RenderCopy to scale it.
  if (texture_ && !textureDirty_) {
    return texture_;
  }

  if (textureDirty_) {
    if (texture_) {
      SDL_DestroyTexture(texture_);
      texture_ = nullptr;
    }

    SDL_RWops *rw = SDL_RWFromConstMem(jpgData_.data(), (int)jpgData_.size());
    if (rw) {
      SDL_Surface *surf = IMG_Load_RW(rw, 1); // 1 = frees src
      if (surf) {
        texture_ = SDL_CreateTextureFromSurface(renderer, surf);
        if (texture_) {
          texW_ = surf->w;
          texH_ = surf->h;
          SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
          LOG_D("CloudProvider", "Created cloud texture {}x{}", texW_, texH_);
        } else {
          LOG_E("CloudProvider", "Failed to create texture: {}",
                SDL_GetError());
        }
        SDL_FreeSurface(surf);
      } else {
        LOG_E("CloudProvider", "Failed to load JPEG: {}", IMG_GetError());
      }
    }
    textureDirty_ = false;
  }

  return texture_;
}
