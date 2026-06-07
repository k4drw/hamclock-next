#include "GribCloudProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

// GRIB2 binary helpers (big-endian)

static inline uint16_t u16be(const uint8_t *p) {
  return ((uint16_t)p[0] << 8) | p[1];
}
static inline uint32_t u32be(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | p[3];
}
static inline uint64_t u64be(const uint8_t *p) {
  return ((uint64_t)u32be(p) << 32) | u32be(p + 4);
}
static inline int16_t i16be(const uint8_t *p) { return (int16_t)u16be(p); }
static inline float ieee754be(const uint8_t *p) {
  uint32_t u = u32be(p);
  float f;
  std::memcpy(&f, &u, 4);
  return f;
}
static uint32_t readBits(const uint8_t *data, size_t bitOffset, int n) {
  if (n == 0)
    return 0;
  size_t byteStart = bitOffset / 8;
  int bitStart = (int)(bitOffset % 8);
  int bytesNeeded = (bitStart + n + 7) / 8;
  uint64_t buf = 0;
  for (int i = 0; i < bytesNeeded; ++i)
    buf = (buf << 8) | data[byteStart + i];
  buf >>= (bytesNeeded * 8 - bitStart - n);
  buf &= (1ULL << n) - 1;
  return (uint32_t)buf;
}

// GRIB2 decoder — Template 5.0 (simple packing) for TCDC only
// discipline=0, parameterCategory=6, parameterNumber=1

SDL_Surface *GribCloudProvider::decodeGrib(const std::vector<uint8_t> &data) {
  size_t pos = 0;
  int nx = 1440, ny = 721;
  std::vector<float> lcdc(nx * ny, 0.0f);
  std::vector<float> mcdc(nx * ny, 0.0f);
  std::vector<float> hcdc(nx * ny, 0.0f);
  bool gotL = false, gotM = false, gotH = false;

  while (pos + 16 <= data.size()) {
    if (data[pos] != 'G' || data[pos + 1] != 'R' || data[pos + 2] != 'I' ||
        data[pos + 3] != 'B') {
      ++pos;
      continue;
    }
    if (data[pos + 7] != 2) {
      pos += 4;
      continue;
    }

    uint64_t msgLen = u64be(data.data() + pos + 8);
    if (msgLen < 16 || pos + msgLen > data.size())
      break;

    size_t msgEnd = pos + msgLen;
    size_t secPos = pos + 16;
    uint8_t discipline = data[pos + 6];

    int msgNx = 0, msgNy = 0;
    uint8_t paramCat = 255, paramNum = 255;
    float R = 0.0f;
    int16_t E = 0, D = 0;
    uint8_t nBits = 0;
    uint32_t nValues = 0;
    bool hasBitmap = false;
    bool got3 = false, got4 = false, got5 = false, got6 = false;

    // Template 5.2/5.3 complex packing state
    enum PackType : uint8_t { PACK_SIMPLE, PACK_COMPLEX, PACK_COMPLEX_SPATIAL };
    PackType packType = PACK_SIMPLE;
    uint8_t missingMgmt = 0;
    uint32_t nGroups = 0;
    uint8_t refGroupWidth = 0, bitsGroupWidth = 0;
    uint32_t refGroupLength = 0;
    uint8_t lengthIncrement = 1;
    uint32_t trueLastLength = 0;
    uint8_t bitsGroupLength = 0;
    uint8_t spatialOrder = 0, octetsExtra = 0;

    while (secPos + 5 <= msgEnd) {
      if (secPos + 4 <= msgEnd && data[secPos] == '7' &&
          data[secPos + 1] == '7' && data[secPos + 2] == '7' &&
          data[secPos + 3] == '7')
        break;

      uint32_t secLen = u32be(data.data() + secPos);
      uint8_t secNum = data[secPos + 4];
      if (secLen < 5 || secPos + secLen > msgEnd)
        break;

      const uint8_t *body = data.data() + secPos + 5;
      size_t bodyLen = secLen - 5;

      switch (secNum) {
      case 3:
        if (bodyLen >= 34 && u16be(body + 7) == 0) {
          msgNx = (int)u32be(body + 25);
          msgNy = (int)u32be(body + 29);
          got3 = (msgNx > 0 && msgNy > 0);
        }
        break;

      case 4:
        if (bodyLen >= 6) {
          paramCat = body[4];
          paramNum = body[5];
          got4 = true;
        }
        break;

      case 5:
        if (bodyLen >= 15) {
          nValues = u32be(body + 0);
          uint16_t tmpl = u16be(body + 4);
          R = ieee754be(body + 6);
          E = i16be(body + 10);
          D = i16be(body + 12);
          nBits = body[14];
          if (tmpl == 0) {
            packType = PACK_SIMPLE;
            got5 = true;
          } else if ((tmpl == 2 || tmpl == 3) && bodyLen >= 42) {
            missingMgmt = body[17];
            nGroups = u32be(body + 26);
            refGroupWidth = body[30];
            bitsGroupWidth = body[31];
            refGroupLength = u32be(body + 32);
            lengthIncrement = body[36] ? body[36] : 1;
            trueLastLength = u32be(body + 37);
            bitsGroupLength = body[41];
            if (tmpl == 3 && bodyLen >= 44) {
              spatialOrder = body[42];
              octetsExtra = body[43];
            }
            packType = (tmpl == 3) ? PACK_COMPLEX_SPATIAL : PACK_COMPLEX;
            got5 = true;
          }
        }
        break;

      case 6:
        if (bodyLen >= 1) {
          hasBitmap = (body[0] == 0);
          got6 = true;
        }
        break;

      case 7:
        if (got3 && got4 && got5 && got6 && !hasBitmap && discipline == 0 &&
            paramCat == 6 && (paramNum == 3 || paramNum == 4 || paramNum == 5)) {

          size_t count = std::min((size_t)msgNx * msgNy, (size_t)nValues);
          std::vector<float> vals;

          if (packType == PACK_SIMPLE && nBits > 0) {
            count = std::min(count, (bodyLen * 8) / nBits);
            vals.resize(count);
            double s2E = std::pow(2.0, (double)E);
            double s10D = std::pow(10.0, (double)D);
            for (size_t i = 0; i < count; ++i) {
              uint32_t raw = readBits(body, i * nBits, nBits);
              vals[i] = (float)((R + raw * s2E) / s10D);
            }
          } else if ((packType == PACK_COMPLEX ||
                      packType == PACK_COMPLEX_SPATIAL) &&
                     nGroups > 0 && missingMgmt == 0) {
            std::vector<int64_t> initVals;
            int64_t minDiff = 0;
            size_t bitPos = 0;

            if (packType == PACK_COMPLEX_SPATIAL && spatialOrder > 0 &&
                octetsExtra > 0) {
              int nExtra = (int)spatialOrder + 1;
              size_t byteOff = 0;
              for (int e = 0; e < nExtra; ++e) {
                uint64_t val = 0;
                for (int b = 0; b < (int)octetsExtra; ++b)
                  val = (val << 8) | body[byteOff++];
                uint64_t signBit = 1ULL << ((int)octetsExtra * 8 - 1);
                int64_t sval = (val & signBit) ? -(int64_t)(val & ~signBit)
                                               : (int64_t)val;
                if (e < (int)spatialOrder)
                  initVals.push_back(sval);
                else
                  minDiff = sval;
              }
              bitPos = byteOff * 8;
            }

            std::vector<uint32_t> X1(nGroups, 0);
            if (nBits > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                X1[g] = readBits(body, bitPos, nBits);
                bitPos += nBits;
              }
            }
            bitPos = (bitPos + 7) & ~7ULL;

            std::vector<uint32_t> W(nGroups, (uint32_t)refGroupWidth);
            if (bitsGroupWidth > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                W[g] = readBits(body, bitPos, bitsGroupWidth) + refGroupWidth;
                bitPos += bitsGroupWidth;
              }
            }
            bitPos = (bitPos + 7) & ~7ULL;

            std::vector<uint32_t> L(nGroups, refGroupLength);
            if (bitsGroupLength > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                L[g] = readBits(body, bitPos, bitsGroupLength) *
                           lengthIncrement +
                       refGroupLength;
                bitPos += bitsGroupLength;
              }
            }
            if (!L.empty())
              L.back() = trueLastLength;
            bitPos = (bitPos + 7) & ~7ULL;

            uint32_t totalVals = 0;
            for (uint32_t g = 0; g < nGroups; ++g)
              totalVals += L[g];
            count = std::min(count, (size_t)totalVals);

            std::vector<int64_t> intVals(count, 0);
            {
              size_t idx = 0;
              for (uint32_t g = 0; g < nGroups && idx < count; ++g) {
                uint32_t len = L[g], w = W[g];
                uint32_t canDo = (uint32_t)std::min((size_t)len, count - idx);
                for (uint32_t k = 0; k < canDo; ++k, ++idx) {
                  intVals[idx] = (w == 0) ? (int64_t)X1[g]
                                          : (int64_t)X1[g] +
                                                (int64_t)readBits(body, bitPos, w);
                  if (w > 0)
                    bitPos += w;
                }
                if (canDo < len && w > 0)
                  bitPos += (uint64_t)(len - canDo) * w;
              }
            }

            if (packType == PACK_COMPLEX_SPATIAL && spatialOrder > 0 &&
                !initVals.empty()) {
              std::vector<int64_t> restored(count, 0);
              if (spatialOrder == 1) {
                restored[0] = initVals[0];
                for (size_t i = 1; i < count; ++i)
                  restored[i] = intVals[i] + minDiff + restored[i - 1];
              } else if (spatialOrder == 2 && initVals.size() >= 2) {
                restored[0] = initVals[0];
                restored[1] = initVals[1];
                for (size_t i = 2; i < count; ++i)
                  restored[i] = intVals[i] + minDiff + (2 * restored[i - 1]) -
                                 restored[i - 2];
              }
              intVals = std::move(restored);
            }

            double s2E = std::pow(2.0, (double)E);
            double s10D = std::pow(10.0, (double)D);
            vals.resize(count);
            for (size_t i = 0; i < count; ++i)
              vals[i] = (float)((R + (double)intVals[i] * s2E) / s10D);
          }

          if (!vals.empty()) {
            if (paramNum == 3) {
              lcdc = std::move(vals);
              gotL = true;
              LOG_I("GribCloud", "Decoded LCDC");
            } else if (paramNum == 4) {
              mcdc = std::move(vals);
              gotM = true;
              LOG_I("GribCloud", "Decoded MCDC");
            } else if (paramNum == 5) {
              hcdc = std::move(vals);
              gotH = true;
              LOG_I("GribCloud", "Decoded HCDC");
            }
          }
        }
        break;

      default:
        break;
      }
      secPos += secLen;
    }
    pos = msgEnd;
  }

  if (!gotL && !gotM && !gotH) {
    LOG_W("GribCloud", "No cloud fields (LCDC/MCDC/HCDC) found in GRIB2 data");
    return nullptr;
  }

  // Build surface at native GRIB resolution (1440x721).
  SDL_Surface *surf =
      SDL_CreateRGBSurfaceWithFormat(0, nx, ny, 32, SDL_PIXELFORMAT_RGBA8888);
  if (!surf)
    return nullptr;

  uint32_t *px = (uint32_t *)surf->pixels;
  int pitch = surf->pitch / 4;

  for (int iy = 0; iy < ny; ++iy) {
    int grib_row = ny - 1 - iy; // flip: surface row 0 = north pole
    for (int ix = 0; ix < nx; ++ix) {
      size_t idx = (size_t)grib_row * nx + ix;
      float l = (gotL && idx < lcdc.size()) ? lcdc[idx] : 0.0f;
      float m = (gotM && idx < mcdc.size()) ? mcdc[idx] : 0.0f;
      float h = (gotH && idx < hcdc.size()) ? hcdc[idx] : 0.0f;

      // Weighted blend from suggestions
      float combined = (0.6f * l + 0.8f * m + 1.0f * h);
      float cloud_fraction = std::clamp(combined / 100.0f, 0.0f, 1.0f);

      // Nonlinear alpha mapping: alpha = pow(cloud_fraction, 1.5)
      uint8_t alpha = (uint8_t)(255.0f * std::pow(cloud_fraction, 1.5f));

      px[iy * pitch + ix] = SDL_MapRGBA(surf->format, 255, 255, 255, alpha);
    }
  }

  return surf;
}

// GFS cycle URL construction (same pattern as WxMbProvider)

std::string GribCloudProvider::buildUrl(int fhr) {
  // Step back 4 hours so we only request a cycle that has been published.
  std::time_t t = std::time(nullptr) - 4 * 3600;
  struct tm gmt{};
#ifdef _WIN32
  gmtime_s(&gmt, &t);
#else
  gmtime_r(&t, &gmt);
#endif
  int hh = (gmt.tm_hour / 6) * 6;
  char buf[1024];
  std::snprintf(buf, sizeof(buf),
                "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl"
                "?file=gfs.t%02dz.pgrb2.0p25.f%03d"
                "&lev_low_cloud_layer=on&lev_middle_cloud_layer=on"
                "&lev_high_cloud_layer=on&var_LCDC=on&var_MCDC=on&var_HCDC=on"
                "&leftlon=0&rightlon=359.75&toplat=90&bottomlat=-90"
                "&dir=%%2Fgfs.%04d%02d%02d%%2F%02d%%2Fatmos",
                hh, fhr, gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday, hh);
  return buf;
}

// Public interface

GribCloudProvider::GribCloudProvider(NetworkManager &net) : net_(net) {}

GribCloudProvider::~GribCloudProvider() {
  for (auto *s : pendingSurfaces_) {
    if (s) SDL_FreeSurface(s);
  }
  pendingSurfaces_.clear();
}

void GribCloudProvider::update() {
  lastFetchMs_ = SDL_GetTicks();
  std::string baseCheckUrl = buildUrl(0);
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (baseCheckUrl == lastUrl_ || baseCheckUrl == fetchingUrl_)
      return;
    fetchingUrl_ = baseCheckUrl;
  }

  LOG_I("GribCloud", "Fetching GFS Clouds animation cycle...");
  std::weak_ptr<GribCloudProvider> self = shared_from_this();

  auto sharedSurfs = std::make_shared<std::vector<SDL_Surface*>>(4, nullptr);
  auto completed = std::make_shared<std::atomic<int>>(0);

  for (int i = 0; i < 4; ++i) {
    int fhr = i * 3;
    std::string url = buildUrl(fhr);
    net_.fetchAsync(
        url,
        [self, baseCheckUrl, sharedSurfs, completed, i](std::string rawData) {
          auto p = self.lock();
          if (!p)
            return;
          if (rawData.empty()) {
            LOG_W("GribCloud", "GRIB2 fetch returned empty for frame {}", i);
            if (completed->fetch_add(1) == 3) {
              std::lock_guard<std::mutex> lk(p->mutex_);
              p->fetchingUrl_ = "";
            }
            return;
          }
          WorkerService::getInstance().submitTask(
              [self, baseCheckUrl, rawData = std::move(rawData), sharedSurfs, completed, i]() {
                auto p2 = self.lock();
                if (!p2)
                  return;

                std::vector<uint8_t> bytes(rawData.begin(), rawData.end());
                SDL_Surface *surf = decodeGrib(bytes);
                if (!surf) {
                  LOG_W("GribCloud", "GRIB2 TCDC decode failed for frame {}", i);
                }
                (*sharedSurfs)[i] = surf;

                if (completed->fetch_add(1) == 3) {
                  std::lock_guard<std::mutex> lk(p2->mutex_);
                  p2->fetchingUrl_ = "";
                  
                  for (auto *s : p2->pendingSurfaces_) {
                    if (s) SDL_FreeSurface(s);
                  }
                  p2->pendingSurfaces_ = *sharedSurfs;
                  p2->lastUrl_ = baseCheckUrl;
                  p2->hasData_ = true;
                  LOG_I("GribCloud", "TCDC animation surfaces ready");
                }
              });
        });
  }
}

std::vector<SDL_Surface*> GribCloudProvider::takeSurfaces() {
  std::lock_guard<std::mutex> lk(mutex_);
  std::vector<SDL_Surface*> surfs = std::move(pendingSurfaces_);
  pendingSurfaces_.clear();
  return surfs;
}

bool GribCloudProvider::hasData() const {
  std::lock_guard<std::mutex> lk(mutex_);
  return hasData_;
}
