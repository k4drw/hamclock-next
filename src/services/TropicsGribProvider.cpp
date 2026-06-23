#include "TropicsGribProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <ctime>
#include <miniz.h>

TropicsGribProvider::TropicsGribProvider(NetworkManager &net) : net_(net) {}

TropicsGribProvider::~TropicsGribProvider() {
  for (auto *s : pendingSurfaces_) {
    if (s) SDL_FreeSurface(s);
  }
  pendingSurfaces_.clear();
}

std::string TropicsGribProvider::buildUrl() {
  time_t t = time(nullptr);
  struct tm gmt;
#ifdef _WIN32
  gmtime_s(&gmt, &t);
#else
  gmtime_r(&t, &gmt);
#endif
  int hh = (gmt.tm_hour / 6) * 6;
  char buf[1024];
  // Example: https://ftp.nhc.ncep.noaa.gov/wsp/2026/06/tpcprblty.2026061912.grib2.gz
  std::snprintf(buf, sizeof(buf),
                "https://ftp.nhc.ncep.noaa.gov/wsp/%04d/%02d/tpcprblty.%04d%02d%02d%02d.grib2.gz",
                gmt.tm_year + 1900, gmt.tm_mon + 1,
                gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday, hh);
  return buf;
}

std::vector<uint8_t> TropicsGribProvider::decompressGz(const std::string &compressed) {
  std::vector<uint8_t> out;
  if (compressed.empty()) return out;

  z_stream zs;
  memset(&zs, 0, sizeof(zs));
  // 16 + MAX_WBITS enables gzip decoding
  if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) return out;

  zs.next_in = (Bytef*)compressed.data();
  zs.avail_in = compressed.size();

  int ret;
  char outbuffer[32768];
  do {
    zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
    zs.avail_out = sizeof(outbuffer);

    ret = inflate(&zs, 0);
    if (out.size() < zs.total_out) {
      out.insert(out.end(), outbuffer, outbuffer + zs.total_out - out.size());
    }
  } while (ret == Z_OK);

  inflateEnd(&zs);

  if (ret != Z_STREAM_END) {
    LOG_E("TropicsGrib", "Failed to decompress gzip, error code: {}", ret);
    out.clear();
  }
  return out;
}

SDL_Surface *TropicsGribProvider::decodeGrib(const std::vector<uint8_t> &data, int forecastHour) {
  // TODO: Implement GRIB2 template parsing for Tropics probabilities, similar to GribCloudProvider::decodeGrib
  // This will extract the parameter for the requested forecastHour and render to a transparent SDL_Surface overlay.
  return nullptr;
}

void TropicsGribProvider::update() {
  lastFetchMs_ = SDL_GetTicks();
  std::string targetUrl = buildUrl();
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (targetUrl == lastUrl_ || targetUrl == fetchingUrl_)
      return;
    fetchingUrl_ = targetUrl;
  }

  LOG_I("TropicsGrib", "Fetching NHC wind speed probabilities...");
  std::weak_ptr<TropicsGribProvider> self = shared_from_this();

  net_.fetchAsync(
      targetUrl,
      [self, targetUrl](std::string rawData) {
        auto p = self.lock();
        if (!p)
          return;
        if (rawData.empty()) {
          LOG_W("TropicsGrib", "GRIB2 fetch returned empty");
          std::lock_guard<std::mutex> lk(p->mutex_);
          p->fetchingUrl_ = "";
          return;
        }

        WorkerService::getInstance().submitTask(
            [self, targetUrl, rawData = std::move(rawData)]() {
              auto p2 = self.lock();
              if (!p2)
                return;

              auto uncompressed = decompressGz(rawData);
              if (uncompressed.empty()) {
                std::lock_guard<std::mutex> lk(p2->mutex_);
                p2->fetchingUrl_ = "";
                return;
              }

              std::vector<SDL_Surface*> newSurfaces;
              // E.g., parse frames for hours +0, +24, +48, +72, +96, +120
              for (int i = 0; i < 6; ++i) {
                int fhr = i * 24;
                SDL_Surface *surf = decodeGrib(uncompressed, fhr);
                if (surf) {
                  newSurfaces.push_back(surf);
                }
              }

              std::lock_guard<std::mutex> lk(p2->mutex_);
              p2->fetchingUrl_ = "";
              for (auto *s : p2->pendingSurfaces_) {
                if (s) SDL_FreeSurface(s);
              }
              p2->pendingSurfaces_ = std::move(newSurfaces);
              p2->lastUrl_ = targetUrl;
              p2->hasData_ = true;
              LOG_I("TropicsGrib", "Tropics probabilities animation surfaces ready");
            });
      });
}

std::vector<SDL_Surface*> TropicsGribProvider::takeSurfaces() {
  std::lock_guard<std::mutex> lk(mutex_);
  std::vector<SDL_Surface*> surfs = std::move(pendingSurfaces_);
  pendingSurfaces_.clear();
  return surfs;
}

bool TropicsGribProvider::hasData() const {
  std::lock_guard<std::mutex> lk(mutex_);
  return hasData_;
}
