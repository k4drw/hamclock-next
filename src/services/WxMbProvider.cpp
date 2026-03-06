#include "WxMbProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

// ---------------------------------------------------------------------------
// GRIB2 binary helpers (big-endian)
// ---------------------------------------------------------------------------

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
// Read n bits from a big-endian packed bit stream at bitOffset.
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

// ---------------------------------------------------------------------------
// GRIB2 decoder — Template 5.0 (simple), 5.2 (complex), 5.3 (complex+spatial)
// ---------------------------------------------------------------------------

bool WxMbProvider::decodeGFS(const std::vector<uint8_t> &data,
                             GribField &out_prmsl, GribField &out_ugrd,
                             GribField &out_vgrd) {
  int decoded = 0;
  size_t pos = 0;

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

    int nx = 0, ny = 0;
    uint8_t paramCat = 255, paramNum = 255;
    float R = 0.0f;
    int16_t E = 0, D = 0;
    uint8_t nBits = 0;
    uint32_t nValues = 0;
    bool hasBitmap = false;
    bool got3 = false, got4 = false, got5 = false, got6 = false;

    enum PackType : uint8_t {
      PACK_NONE,
      PACK_SIMPLE,
      PACK_COMPLEX,
      PACK_COMPLEX_SPATIAL
    };
    PackType packType = PACK_NONE;
    uint8_t missingMgmt = 0;
    uint32_t nGroups = 0;
    uint8_t refGroupWidth = 0;
    uint8_t bitsGroupWidth = 0;
    uint32_t refGroupLength = 0;
    uint8_t lengthIncrement = 1;
    uint32_t trueLastLength = 0;
    uint8_t bitsGroupLength = 0;
    uint8_t spatialOrder = 0;
    uint8_t octetsExtra = 0;

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
      case 1:
      case 2:
        break;

      case 3:
        if (bodyLen >= 34 && u16be(body + 7) == 0) {
          nx = (int)u32be(body + 25);
          ny = (int)u32be(body + 29);
          got3 = (nx > 0 && ny > 0);
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
        if (got3 && got4 && got5 && got6 && !hasBitmap) {
          GribField field;
          field.nx = nx;
          field.ny = ny;
          size_t count = (size_t)nx * ny;
          if (nValues > 0)
            count = std::min(count, (size_t)nValues);

          if (packType == PACK_SIMPLE && nBits > 0) {
            count = std::min(count, (bodyLen * 8) / nBits);
            field.values.resize(count);
            double s2E = std::pow(2.0, (double)E);
            double s10D = std::pow(10.0, (double)D);
            for (size_t i = 0; i < count; ++i) {
              uint32_t raw = readBits(body, i * nBits, nBits);
              field.values[i] = (float)((R + raw * s2E) / s10D);
            }

          } else if ((packType == PACK_COMPLEX ||
                      packType == PACK_COMPLEX_SPATIAL) &&
                     nGroups > 0 && missingMgmt == 0) {

            // Extra descriptors for spatial differencing (Template 5.3)
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
                // All extra values use sign-magnitude encoding (WMO GRIB2 spec)
                uint64_t signBit = 1ULL << ((int)octetsExtra * 8 - 1);
                int64_t sval =
                    (val & signBit) ? -(int64_t)(val & ~signBit) : (int64_t)val;
                if (e < (int)spatialOrder) {
                  initVals.push_back(sval);
                } else {
                  minDiff = sval;
                }
              }
              bitPos = byteOff * 8;
            }

            // Group reference values (X1)
            std::vector<uint32_t> X1(nGroups, 0);
            if (nBits > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                X1[g] = readBits(body, bitPos, nBits);
                bitPos += nBits;
              }
            }
            bitPos = (bitPos + 7) & ~7ULL; // Align block

            // Group widths
            std::vector<uint32_t> W(nGroups, (uint32_t)refGroupWidth);
            if (bitsGroupWidth > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                W[g] = readBits(body, bitPos, bitsGroupWidth) + refGroupWidth;
                bitPos += bitsGroupWidth;
              }
            }
            bitPos = (bitPos + 7) & ~7ULL; // Align block

            // Group lengths
            std::vector<uint32_t> L(nGroups, refGroupLength);
            if (bitsGroupLength > 0) {
              for (uint32_t g = 0; g < nGroups; ++g) {
                L[g] =
                    readBits(body, bitPos, bitsGroupLength) * lengthIncrement +
                    refGroupLength;
                bitPos += bitsGroupLength;
              }
            }
            if (!L.empty())
              L.back() = trueLastLength;
            bitPos = (bitPos + 7) & ~7ULL; // Align block

            uint32_t totalVals = 0;
            for (uint32_t g = 0; g < nGroups; ++g)
              totalVals += L[g];
            LOG_I("WxMb", "  nGroups={} totalVals={} nValues={} count={}",
                  nGroups, totalVals, nValues, count);
            count = std::min(count, (size_t)totalVals);

            // Packed values → integers
            std::vector<int64_t> intVals(count, 0);
            {
              size_t idx = 0;
              for (uint32_t g = 0; g < nGroups && idx < count; ++g) {
                uint32_t len = L[g];
                uint32_t w = W[g];
                uint32_t canDo = (uint32_t)std::min((size_t)len, count - idx);
                for (uint32_t k = 0; k < canDo; ++k, ++idx) {
                  if (w == 0) {
                    intVals[idx] = (int64_t)X1[g];
                  } else {
                    intVals[idx] =
                        (int64_t)X1[g] + (int64_t)readBits(body, bitPos, w);
                    bitPos += w;
                  }
                }
                if (canDo < len && w > 0)
                  bitPos += (uint64_t)(len - canDo) * w;
              }
            }

            // Spatial un-differencing (Template 5.3)
            // algorithm per WMO GRIB2 spec / wgrib2 g2_unpack3.c:
            //   order=1: restored[0]=IV[0],
            //   restored[n]=restored[n-1]+intVals[n]+minDiff order=2:
            //   restored[0]=IV[0], restored[1]=IV[1],
            //            restored[n]=2*restored[n-1]-restored[n-2]+intVals[n]+minDiff
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
                for (size_t i = 2; i < count; ++i) {
                  // GRIB2 spec logic: skip first N points in bitstream or use
                  // them as dummy. grib2dec logic: bitstream contains dummy
                  // values for i < spatialOrder. We overwrite indices 0 and 1
                  // with initVals, but we must use intVals[i] correctly.
                  restored[i] = intVals[i] + minDiff + (2 * restored[i - 1]) -
                                restored[i - 2];
                }
              }
              intVals = std::move(restored);
            }

            // Convert to physical: (R + int * 2^E) / 10^D
            double s2E = std::pow(2.0, (double)E);
            double s10D = std::pow(10.0, (double)D);
            field.values.resize(count);
            for (size_t i = 0; i < count; ++i)
              field.values[i] = (float)((R + (double)intVals[i] * s2E) / s10D);
          }

          if (!field.values.empty()) {
            LOG_I("WxMb",
                  "GRIB field: disc={} cat={} num={} nx={} ny={} vals={}",
                  discipline, paramCat, paramNum, field.nx, field.ny,
                  field.values.size());
            if (discipline == 0 && paramCat == 3 && paramNum == 1) {
              for (auto &v : field.values)
                v /= 100.0f; // Pa → hPa
              out_prmsl = std::move(field);
              ++decoded;
            } else if (discipline == 0 && paramCat == 2 && paramNum == 2) {
              out_ugrd = std::move(field);
              ++decoded;
            } else if (discipline == 0 && paramCat == 2 && paramNum == 3) {
              out_vgrd = std::move(field);
              ++decoded;
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
    if (decoded >= 3)
      break;
  }

  return decoded >= 3 && !out_prmsl.values.empty() &&
         !out_ugrd.values.empty() && !out_vgrd.values.empty();
}

// ---------------------------------------------------------------------------
// Marching squares — segment table
// bit0=TL, bit1=TR, bit2=BR, bit3=BL
// Edges: 0=top(TL-TR), 1=right(TR-BR), 2=bottom(BR-BL), 3=left(BL-TL)
// ---------------------------------------------------------------------------
static const int8_t MC_SEGS[16][2][2] = {
    {{-1, -1}, {-1, -1}}, // 0  all outside
    {{0, 3}, {-1, -1}},   // 1  TL
    {{0, 1}, {-1, -1}},   // 2  TR
    {{1, 3}, {-1, -1}},   // 3  TL+TR
    {{1, 2}, {-1, -1}},   // 4  BR
    {{0, 3}, {1, 2}},     // 5  TL+BR (saddle)
    {{0, 2}, {-1, -1}},   // 6  TR+BR
    {{2, 3}, {-1, -1}},   // 7  TL+TR+BR
    {{2, 3}, {-1, -1}},   // 8  BL
    {{0, 2}, {-1, -1}},   // 9  TL+BL
    {{0, 1}, {2, 3}},     // 10 TR+BL (saddle)
    {{1, 2}, {-1, -1}},   // 11 TL+TR+BL
    {{1, 3}, {-1, -1}},   // 12 BR+BL
    {{0, 1}, {-1, -1}},   // 13 TL+BR+BL
    {{0, 3}, {-1, -1}},   // 14 TR+BR+BL
    {{-1, -1}, {-1, -1}}, // 15 all inside
};

// ---------------------------------------------------------------------------
// buildSegments — marching squares + quivers → lat/lon geometry
// ---------------------------------------------------------------------------

void WxMbProvider::buildSegments(const GribField &prmsl, const GribField &ugrd,
                                 const GribField &vgrd, float pMin, float pMax,
                                 int W, int H, std::vector<WxSegment> &segs,
                                 std::vector<WxQuiver> &quivers,
                                 SDL_Surface *&fillSurface) {
  segs.clear();
  quivers.clear();
  fillSurface = nullptr;
  if (prmsl.values.empty())
    return;

  // Bicubic B-spline sampler shared by both isobars and quivers.
  auto cubic = [](float v0, float v1, float v2, float v3, float x) {
    float a = -0.5f * v0 + 1.5f * v1 - 1.5f * v2 + 0.5f * v3;
    float b = v0 - 2.5f * v1 + 2.0f * v2 - 0.5f * v3;
    float c = -0.5f * v0 + 0.5f * v2;
    float d = v1;
    return a * x * x * x + b * x * x + c * x + d;
  };

  auto sampleField = [&](const GribField &f, float fx, float fy) -> float {
    if (f.values.empty())
      return 0.0f;
    // GFS 0–360°E grid; equirectangular map shows -180°–+180°.
    // Left pixel (fx=0) = -180° = 180°E = GFS col nx/2.
    float x_scaled = (fx / (float)W) * (float)f.nx + (float)f.nx / 2.0f;
    float y_scaled = (fy / (float)H) * (float)f.ny;

    float xf = std::fmod(x_scaled, (float)f.nx);
    if (xf < 0)
      xf += f.nx;
    float yf = std::clamp(y_scaled, 0.0f, (float)f.ny - 1.0f);

    int ix = (int)std::floor(xf);
    int iy = (int)std::floor(yf);
    float tx = xf - ix;
    float ty = yf - iy;

    float p[4][4];
    for (int j = -1; j <= 2; ++j) {
      int y = std::clamp(iy + j, 0, f.ny - 1);
      for (int i = -1; i <= 2; ++i) {
        int x = (ix + i + f.nx) % f.nx;
        p[j + 1][i + 1] = f.values[y * f.nx + x];
      }
    }
    float col[4];
    for (int i = 0; i < 4; ++i)
      col[i] = cubic(p[i][0], p[i][1], p[i][2], p[i][3], tx);
    return cubic(col[0], col[1], col[2], col[3], ty);
  };

  // Convert surface pixel → geographic coordinates.
  auto toGeo = [&](float sx, float sy, float &lon, float &lat) {
    lon = (sx / (float)W) * 360.0f - 180.0f;
    lat = 90.0f - (sy / (float)H) * 180.0f;
  };

  // ---- Isobar contours (marching squares) ---------------------------------
  float lo = std::floor(pMin / 4.0f) * 4.0f;
  float hi = std::ceil(pMax / 4.0f) * 4.0f;
  lo = std::max(lo, 880.0f);
  hi = std::min(hi, 1080.0f);

  const float s = 2.0f; // cell size (pixels in virtual 1024×512 space)
  for (float level = lo; level <= hi; level += 4.0f) {
    for (float cy = 0; cy < (float)H - s; cy += s) {
      for (float cx = 0; cx < (float)W - s; cx += s) {
        float v0 = sampleField(prmsl, cx, cy);
        float v1 = sampleField(prmsl, cx + s, cy);
        float v2 = sampleField(prmsl, cx + s, cy + s);
        float v3 = sampleField(prmsl, cx, cy + s);

        int mask = ((v0 >= level) ? 1 : 0) | ((v1 >= level) ? 2 : 0) |
                   ((v2 >= level) ? 4 : 0) | ((v3 >= level) ? 8 : 0);
        if (mask == 0 || mask == 15)
          continue;

        auto interp = [](float va, float vb, float lev) -> float {
          float d = vb - va;
          return (std::abs(d) < 1e-4f) ? 0.5f
                                       : std::clamp((lev - va) / d, 0.0f, 1.0f);
        };
        auto edgePt = [&](int edge, float &ox, float &oy) {
          switch (edge) {
          case 0:
            ox = cx + interp(v0, v1, level) * s;
            oy = cy;
            break;
          case 1:
            ox = cx + s;
            oy = cy + interp(v1, v2, level) * s;
            break;
          case 2:
            ox = cx + (1.0f - interp(v2, v3, level)) * s;
            oy = cy + s;
            break;
          case 3:
            ox = cx;
            oy = cy + (1.0f - interp(v3, v0, level)) * s;
            break;
          default:
            ox = cx;
            oy = cy;
            break;
          }
        };

        for (int si = 0; si < 2; ++si) {
          int ea = MC_SEGS[mask][si][0];
          int eb = MC_SEGS[mask][si][1];
          if (ea < 0)
            break;
          float ax, ay, bx, by;
          edgePt(ea, ax, ay);
          edgePt(eb, bx, by);
          WxSegment seg;
          toGeo(ax, ay, seg.lon1, seg.lat1);
          toGeo(bx, by, seg.lon2, seg.lat2);
          segs.push_back(seg);
        }
      }
    }
  }

  // ---- Wind quivers --------------------------------------------------------
  if (ugrd.nx > 0 && vgrd.nx > 0) {
    const int step = 25;
    for (int ay = step / 2; ay < H; ay += step) {
      for (int ax = step / 2; ax < W; ax += step) {
        float u = sampleField(ugrd, (float)ax, (float)ay);
        float v = sampleField(vgrd, (float)ax, (float)ay);
        float speed = std::sqrt(u * u + v * v);
        if (speed < 0.5f)
          continue;
        WxQuiver q;
        toGeo((float)ax, (float)ay, q.lon, q.lat);
        q.u = u;
        q.v = v;
        quivers.push_back(q);
      }
    }
  }

  LOG_I("WxMb", "buildSegments: {} isobar segs, {} quivers", (int)segs.size(),
        (int)quivers.size());

  // ---- Pressure fill raster (360x180, one pixel per degree) ---------------
  // Map pressure → RGBA using a Zoom-Earth-style blue–white–red ramp.
  // Color stops (hPa → RGBA):
  //   ≤960: deep blue   (20, 40, 200, 170)
  //   990   mid blue    (100, 160, 240, 150)
  //   1013.25 standard  (240, 240, 248, 110)  near-white/neutral
  //   1025  warm peach  (255, 160, 110, 150)
  //   ≥1040 deep red    (195, 25, 25, 170)
  struct Stop {
    float hPa;
    uint8_t r, g, b, a;
  };
  static const Stop kStops[] = {
      {960.0f, 20, 40, 200, 170},     {990.0f, 100, 160, 240, 150},
      {1013.25f, 240, 240, 248, 110}, {1025.0f, 255, 160, 110, 150},
      {1040.0f, 195, 25, 25, 170},
  };
  static constexpr int kNStops = 5;

  auto pressureToColor = [&](float hpa) -> SDL_Color {
    if (hpa <= kStops[0].hPa)
      return {kStops[0].r, kStops[0].g, kStops[0].b, kStops[0].a};
    if (hpa >= kStops[kNStops - 1].hPa)
      return {kStops[kNStops - 1].r, kStops[kNStops - 1].g,
              kStops[kNStops - 1].b, kStops[kNStops - 1].a};
    for (int i = 0; i < kNStops - 1; ++i) {
      if (hpa <= kStops[i + 1].hPa) {
        float t = (hpa - kStops[i].hPa) / (kStops[i + 1].hPa - kStops[i].hPa);
        auto lerp8 = [&](uint8_t a, uint8_t b) -> uint8_t {
          return (uint8_t)(a + (b - a) * t);
        };
        return {lerp8(kStops[i].r, kStops[i + 1].r),
                lerp8(kStops[i].g, kStops[i + 1].g),
                lerp8(kStops[i].b, kStops[i + 1].b),
                lerp8(kStops[i].a, kStops[i + 1].a)};
      }
    }
    return {240, 240, 248, 110};
  };

  constexpr int FW = 360, FH = 180;
  SDL_Surface *fill =
      SDL_CreateRGBSurfaceWithFormat(0, FW, FH, 32, SDL_PIXELFORMAT_RGBA8888);
  if (fill) {
    uint32_t *px = (uint32_t *)fill->pixels;
    int pitch = fill->pitch / 4;
    float scaleX = (float)W / FW;
    float scaleY = (float)H / FH;
    for (int iy = 0; iy < FH; ++iy) {
      for (int ix = 0; ix < FW; ++ix) {
        float p = sampleField(prmsl, ix * scaleX, iy * scaleY);
        SDL_Color c = pressureToColor(p);
        px[iy * pitch + ix] = SDL_MapRGBA(fill->format, c.r, c.g, c.b, c.a);
      }
    }
    fillSurface = fill;
  }
}

// ---------------------------------------------------------------------------
// GFS cycle URL construction
// ---------------------------------------------------------------------------

std::string WxMbProvider::buildNomadsUrl() {
  std::time_t t = std::time(nullptr) - 4 * 3600;
  struct tm gmt{};
#ifdef _WIN32
  gmtime_s(&gmt, &t);
#else
  gmtime_r(&t, &gmt);
#endif
  int hh = (gmt.tm_hour / 6) * 6;
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "https://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_0p25.pl"
                "?file=gfs.t%02dz.pgrb2.0p25.f000"
                "&lev_mean_sea_level=on&lev_10_m_above_ground=on"
                "&var_PRMSL=on&var_UGRD=on&var_VGRD=on"
                "&leftlon=0&rightlon=359.75&toplat=90&bottomlat=-90"
                "&dir=%%2Fgfs.%04d%02d%02d%%2F%02d%%2Fatmos",
                hh, gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday, hh);
  return buf;
}

// ---------------------------------------------------------------------------
// WxMbProvider public interface
// ---------------------------------------------------------------------------

WxMbProvider::WxMbProvider(NetworkManager &net) : net_(net) {}
WxMbProvider::~WxMbProvider() {
  if (pendingFillSurface_)
    SDL_FreeSurface(pendingFillSurface_);
}

void WxMbProvider::update() {
  std::string url = buildNomadsUrl();
  {
    std::lock_guard<std::mutex> lk(mutex_);
    if (url == lastUrl_ || url == fetchingUrl_)
      return;
    fetchingUrl_ = url;
  }

  LOG_I("WxMb", "Fetching GFS WX subset: {}", url);
  net_.fetchAsync(
      url,
      [this, url](std::string rawData) {
        if (rawData.empty()) {
          LOG_W("WxMb", "GFS GRIB2 fetch returned empty response");
          std::lock_guard<std::mutex> lk(mutex_);
          fetchingUrl_ = "";
          return;
        }
        WorkerService::getInstance().submitTask(
            [this, url, rawData = std::move(rawData)]() {
              std::vector<uint8_t> bytes(rawData.begin(), rawData.end());

              GribField prmsl, ugrd, vgrd;
              if (!decodeGFS(bytes, prmsl, ugrd, vgrd)) {
                LOG_W("WxMb", "GRIB2 decode failed");
                std::lock_guard<std::mutex> lk(mutex_);
                fetchingUrl_ = "";
                return;
              }

              auto minmax =
                  [](const std::vector<float> &v) -> std::pair<float, float> {
                if (v.empty())
                  return {0, 0};
                float mn = v[0], mx = v[0];
                for (auto x : v) {
                  if (x < mn)
                    mn = x;
                  if (x > mx)
                    mx = x;
                }
                return {mn, mx};
              };

              auto [pmn, pmx] = minmax(prmsl.values);
              auto [umn, umx] = minmax(ugrd.values);
              auto [vmn, vmx] = minmax(vgrd.values);
              LOG_I("WxMb",
                    "GFS decoded: {}pt PRMSL [{:.1f},{:.1f}], {}pt UGRD "
                    "[{:.1f},{:.1f}], {}pt VGRD [{:.1f},{:.1f}]",
                    (int)prmsl.values.size(), pmn, pmx, (int)ugrd.values.size(),
                    umn, umx, (int)vgrd.values.size(), vmn, vmx);

              std::vector<WxSegment> segs;
              std::vector<WxQuiver> quivers;
              SDL_Surface *fillSurf = nullptr;
              buildSegments(prmsl, ugrd, vgrd, pmn, pmx, 1024, 512, segs,
                            quivers, fillSurf);

              std::lock_guard<std::mutex> lk(mutex_);
              segments_ = std::move(segs);
              quivers_ = std::move(quivers);
              if (pendingFillSurface_)
                SDL_FreeSurface(pendingFillSurface_);
              pendingFillSurface_ = fillSurf;
              segmentsDirty_ = true;
              hasData_ = true;
              lastUrl_ = url;
              fetchingUrl_ = "";
              lastUpdateMs_ = (uint64_t)SDL_GetTicks();
            });
      },
      0);
}

bool WxMbProvider::getSegments(std::vector<WxSegment> &segs,
                               std::vector<WxQuiver> &quivers,
                               SDL_Surface *&fillSurface) {
  std::lock_guard<std::mutex> lk(mutex_);
  if (!segmentsDirty_) {
    fillSurface = nullptr;
    return false;
  }
  segs = segments_;
  quivers = quivers_;
  fillSurface = pendingFillSurface_; // transfer ownership to caller
  pendingFillSurface_ = nullptr;
  segmentsDirty_ = false;
  return true;
}

bool WxMbProvider::hasData() const {
  std::lock_guard<std::mutex> lk(mutex_);
  return hasData_;
}

uint64_t WxMbProvider::getLastUpdateMs() const {
  std::lock_guard<std::mutex> lk(mutex_);
  return lastUpdateMs_;
}
