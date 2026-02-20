#include "WxMbProvider.h"
#include "../core/Logger.h"
#include "../core/WorkerService.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>
#include <cstdio>

// ---------------------------------------------------------------------------
// GRIB2 binary helpers (big-endian)
// ---------------------------------------------------------------------------

static inline uint16_t u16be(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}
static inline uint32_t u32be(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static inline uint64_t u64be(const uint8_t* p) {
    return ((uint64_t)u32be(p) << 32) | u32be(p + 4);
}
static inline int16_t i16be(const uint8_t* p) {
    return (int16_t)u16be(p);
}
static inline float ieee754be(const uint8_t* p) {
    uint32_t u = u32be(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
// Read n bits from a big-endian packed bit stream at bitOffset.
static uint32_t readBits(const uint8_t* data, size_t bitOffset, int n) {
    if (n == 0) return 0;
    size_t byteStart  = bitOffset / 8;
    int    bitStart   = (int)(bitOffset % 8);
    int    bytesNeeded = (bitStart + n + 7) / 8;
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

bool WxMbProvider::decodeGFS(const std::vector<uint8_t>& data,
                              GribField& out_prmsl,
                              GribField& out_ugrd,
                              GribField& out_vgrd) {
    int decoded = 0;
    size_t pos  = 0;

    while (pos + 16 <= data.size() && decoded < 3) {
        if (data[pos]!='G'||data[pos+1]!='R'||data[pos+2]!='I'||data[pos+3]!='B') {
            ++pos; continue;
        }
        if (data[pos + 7] != 2) { pos += 4; continue; }

        uint64_t msgLen = u64be(data.data() + pos + 8);
        if (msgLen < 16 || pos + msgLen > data.size()) break;

        size_t  msgEnd    = pos + msgLen;
        size_t  secPos    = pos + 16;
        uint8_t discipline = data[pos + 6];

        int      nx = 0, ny = 0;
        uint8_t  paramCat = 255, paramNum = 255;
        float    R = 0.0f;
        int16_t  E = 0, D = 0;
        uint8_t  nBits = 0;
        uint32_t nValues = 0;
        bool     hasBitmap = false;
        bool     got3=false, got4=false, got5=false, got6=false;

        enum PackType : uint8_t { PACK_NONE, PACK_SIMPLE, PACK_COMPLEX, PACK_COMPLEX_SPATIAL };
        PackType packType = PACK_NONE;
        uint8_t  missingMgmt     = 0;
        uint32_t nGroups         = 0;
        uint8_t  refGroupWidth   = 0;
        uint8_t  bitsGroupWidth  = 0;
        uint32_t refGroupLength  = 0;
        uint8_t  lengthIncrement = 1;
        uint32_t trueLastLength  = 0;
        uint8_t  bitsGroupLength = 0;
        uint8_t  spatialOrder    = 0;
        uint8_t  octetsExtra     = 0;

        while (secPos + 5 <= msgEnd) {
            if (secPos + 4 <= msgEnd &&
                data[secPos]=='7' && data[secPos+1]=='7' &&
                data[secPos+2]=='7' && data[secPos+3]=='7') break;

            uint32_t secLen = u32be(data.data() + secPos);
            uint8_t  secNum = data[secPos + 4];
            if (secLen < 5 || secPos + secLen > msgEnd) break;

            const uint8_t* body    = data.data() + secPos + 5;
            size_t         bodyLen = secLen - 5;

            switch (secNum) {
            case 1: case 2: break;

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
                    nValues       = u32be(body + 0);
                    uint16_t tmpl = u16be(body + 4);
                    R     = ieee754be(body + 6);
                    E     = i16be(body + 10);
                    D     = i16be(body + 12);
                    nBits = body[14];
                    if (tmpl == 0) {
                        packType = PACK_SIMPLE;
                        got5 = true;
                    } else if ((tmpl == 2 || tmpl == 3) && bodyLen >= 42) {
                        missingMgmt     = body[17];
                        nGroups         = u32be(body + 26);
                        refGroupWidth   = body[30];
                        bitsGroupWidth  = body[31];
                        refGroupLength  = u32be(body + 32);
                        lengthIncrement = body[36] ? body[36] : 1;
                        trueLastLength  = u32be(body + 37);
                        bitsGroupLength = body[41];
                        if (tmpl == 3 && bodyLen >= 44) {
                            spatialOrder = body[42];
                            octetsExtra  = body[43];
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
                    if (nValues > 0) count = std::min(count, (size_t)nValues);

                    if (packType == PACK_SIMPLE && nBits > 0) {
                        count = std::min(count, (bodyLen * 8) / nBits);
                        field.values.resize(count);
                        double s2E  = std::pow(2.0, (double)E);
                        double s10D = std::pow(10.0, (double)D);
                        for (size_t i = 0; i < count; ++i) {
                            uint32_t raw = readBits(body, i * nBits, nBits);
                            field.values[i] = (float)((R + raw * s2E) / s10D);
                        }

                    } else if ((packType == PACK_COMPLEX || packType == PACK_COMPLEX_SPATIAL)
                               && nGroups > 0 && missingMgmt == 0) {

                        // Extra descriptors for spatial differencing (Template 5.3)
                        std::vector<int64_t> initVals;
                        int64_t minDiff = 0;
                        size_t  bitPos  = 0;

                        if (packType == PACK_COMPLEX_SPATIAL
                            && spatialOrder > 0 && octetsExtra > 0) {
                            int    nExtra  = (int)spatialOrder + 1;
                            size_t byteOff = 0;
                            for (int e = 0; e < nExtra; ++e) {
                                uint64_t val = 0;
                                for (int b = 0; b < (int)octetsExtra; ++b)
                                    val = (val << 8) | body[byteOff++];
                                if (e < (int)spatialOrder) {
                                    initVals.push_back((int64_t)val);
                                } else {
                                    // Sign-magnitude encoding: MSB = sign
                                    uint64_t signBit = 1ULL << ((int)octetsExtra * 8 - 1);
                                    if (val & signBit)
                                        minDiff = -(int64_t)(val & ~signBit);
                                    else
                                        minDiff = (int64_t)val;
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

                        // Group widths
                        std::vector<uint32_t> W(nGroups, (uint32_t)refGroupWidth);
                        if (bitsGroupWidth > 0) {
                            for (uint32_t g = 0; g < nGroups; ++g) {
                                W[g] = readBits(body, bitPos, bitsGroupWidth) + refGroupWidth;
                                bitPos += bitsGroupWidth;
                            }
                        }

                        // Group lengths
                        std::vector<uint32_t> L(nGroups, refGroupLength);
                        if (bitsGroupLength > 0) {
                            for (uint32_t g = 0; g < nGroups; ++g) {
                                L[g] = readBits(body, bitPos, bitsGroupLength)
                                       * lengthIncrement + refGroupLength;
                                bitPos += bitsGroupLength;
                            }
                        }
                        if (!L.empty()) L.back() = trueLastLength;

                        uint32_t totalVals = 0;
                        for (uint32_t g = 0; g < nGroups; ++g) totalVals += L[g];
                        count = std::min(count, (size_t)totalVals);

                        // Packed values → integers
                        std::vector<int64_t> intVals(count);
                        {
                            size_t idx = 0;
                            for (uint32_t g = 0; g < nGroups && idx < count; ++g) {
                                uint32_t len   = L[g];
                                uint32_t w     = W[g];
                                uint32_t canDo = (uint32_t)std::min((size_t)len, count - idx);
                                for (uint32_t k = 0; k < canDo; ++k, ++idx) {
                                    if (w == 0) {
                                        intVals[idx] = (int64_t)X1[g];
                                    } else {
                                        intVals[idx] = (int64_t)X1[g]
                                            + (int64_t)readBits(body, bitPos, w);
                                        bitPos += w;
                                    }
                                }
                                if (canDo < len && w > 0)
                                    bitPos += (uint64_t)(len - canDo) * w;
                            }
                        }

                        // Spatial un-differencing (Template 5.3)
                        // algorithm per WMO GRIB2 spec / wgrib2 g2_unpack3.c:
                        //   order=1: restored[0]=IV[0], restored[n]=restored[n-1]+intVals[n]+minDiff
                        //   order=2: restored[0]=IV[0], restored[1]=IV[1],
                        //            restored[n]=2*restored[n-1]-restored[n-2]+intVals[n]+minDiff
                        if (packType == PACK_COMPLEX_SPATIAL
                            && spatialOrder > 0 && !initVals.empty()) {
                            std::vector<int64_t> restored(count, 0);
                            if (spatialOrder == 1 && count > 0) {
                                restored[0] = initVals[0];
                                for (size_t i = 1; i < count; ++i)
                                    restored[i] = restored[i-1] + intVals[i] + minDiff;
                            } else if (spatialOrder == 2
                                       && initVals.size() >= 2 && count >= 2) {
                                restored[0] = initVals[0];
                                restored[1] = initVals[1];
                                for (size_t i = 2; i < count; ++i)
                                    restored[i] = 2*restored[i-1] - restored[i-2]
                                                  + intVals[i] + minDiff;
                            }
                            intVals = std::move(restored);
                        }

                        // Convert to physical: (R + int * 2^E) / 10^D
                        double s2E  = std::pow(2.0, (double)E);
                        double s10D = std::pow(10.0, (double)D);
                        field.values.resize(count);
                        for (size_t i = 0; i < count; ++i)
                            field.values[i] = (float)((R + (double)intVals[i] * s2E) / s10D);
                    }

                    if (!field.values.empty()) {
                        if (discipline == 0 && paramCat == 3 && paramNum == 1) {
                            for (auto& v : field.values) v /= 100.0f; // Pa → hPa
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

            default: break;
            }
            secPos += secLen;
        }
        pos = msgEnd;
    }

    return decoded >= 3 &&
           !out_prmsl.values.empty() &&
           !out_ugrd.values.empty()  &&
           !out_vgrd.values.empty();
}

// ---------------------------------------------------------------------------
// Marching squares contour + wind quiver rendering
// ---------------------------------------------------------------------------

// Segment table: bit0=TL, bit1=TR, bit2=BR, bit3=BL
// Edges: 0=top(TL-TR), 1=right(TR-BR), 2=bottom(BR-BL), 3=left(BL-TL)
static const int8_t MC_SEGS[16][2][2] = {
    {{-1,-1},{-1,-1}}, // 0  all outside
    {{ 0, 3},{-1,-1}}, // 1  TL
    {{ 0, 1},{-1,-1}}, // 2  TR
    {{ 1, 3},{-1,-1}}, // 3  TL+TR
    {{ 1, 2},{-1,-1}}, // 4  BR
    {{ 0, 3},{ 1, 2}}, // 5  TL+BR (saddle: split variant)
    {{ 0, 2},{-1,-1}}, // 6  TR+BR
    {{ 2, 3},{-1,-1}}, // 7  TL+TR+BR
    {{ 2, 3},{-1,-1}}, // 8  BL
    {{ 0, 2},{-1,-1}}, // 9  TL+BL
    {{ 0, 1},{ 2, 3}}, // 10 TR+BL (saddle)
    {{ 1, 2},{-1,-1}}, // 11 TL+TR+BL
    {{ 1, 3},{-1,-1}}, // 12 BR+BL
    {{ 0, 1},{-1,-1}}, // 13 TL+BR+BL
    {{ 0, 3},{-1,-1}}, // 14 TR+BR+BL
    {{-1,-1},{-1,-1}}, // 15 all inside
};

static void plotPixel(uint32_t* px, int pitch, int W, int H,
                      int x, int y, uint32_t col) {
    if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H)
        px[y * pitch + x] = col;
}

static void drawLine(uint32_t* px, int pitch, int W, int H,
                     int x0, int y0, int x1, int y1, uint32_t col) {
    int dx = std::abs(x1-x0), sx = (x0<x1)?1:-1;
    int dy = -std::abs(y1-y0), sy = (y0<y1)?1:-1;
    int err = dx + dy;
    for (;;) {
        plotPixel(px, pitch, W, H, x0, y0, col);
        if (x0==x1 && y0==y1) break;
        int e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

SDL_Surface* WxMbProvider::renderToSurface(const GribField& prmsl,
                                            const GribField& ugrd,
                                            const GribField& vgrd,
                                            int W, int H) {
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32,
                                                       SDL_PIXELFORMAT_RGBA8888);
    if (!surf) return nullptr;
    SDL_FillRect(surf, nullptr, 0); // fully transparent

    uint32_t* pixels = static_cast<uint32_t*>(surf->pixels);
    int pitch = surf->pitch / 4;

    const int gW = prmsl.nx, gH = prmsl.ny;

    auto sampleP = [&](int x, int y) -> float {
        int gx = std::clamp(x * gW / W, 0, gW - 1);
        int gy = std::clamp(y * gH / H, 0, gH - 1);
        return prmsl.values[gy * gW + gx];
    };

    uint32_t contourCol = SDL_MapRGBA(surf->format, 255, 255, 255, 200);

    // Marching squares: 960–1040 hPa every 4 hPa (21 levels)
    for (float level = 960.0f; level <= 1040.0f; level += 4.0f) {
        for (int cy = 0; cy < H - 1; ++cy) {
            for (int cx = 0; cx < W - 1; ++cx) {
                float v0 = sampleP(cx,   cy  ); // TL
                float v1 = sampleP(cx+1, cy  ); // TR
                float v2 = sampleP(cx+1, cy+1); // BR
                float v3 = sampleP(cx,   cy+1); // BL

                int mask = ((v0>=level)?1:0) | ((v1>=level)?2:0) |
                           ((v2>=level)?4:0) | ((v3>=level)?8:0);
                if (mask == 0 || mask == 15) continue;

                // Compute edge crossing position
                auto interp = [](float va, float vb, float lev) -> float {
                    float d = vb - va;
                    return (std::abs(d) < 1e-4f) ? 0.5f
                           : std::clamp((lev - va) / d, 0.0f, 1.0f);
                };
                auto edgePt = [&](int edge, float& ox, float& oy) {
                    switch (edge) {
                    case 0: ox = cx + interp(v0,v1,level); oy = (float)cy;   break;
                    case 1: ox = (float)(cx+1); oy = cy + interp(v1,v2,level); break;
                    case 2: ox = cx+1 - interp(v2,v3,level); oy = (float)(cy+1); break;
                    case 3: ox = (float)cx; oy = cy+1 - interp(v3,v0,level); break;
                    default: ox = (float)cx; oy = (float)cy; break;
                    }
                };

                for (int s = 0; s < 2; ++s) {
                    int ea = MC_SEGS[mask][s][0];
                    int eb = MC_SEGS[mask][s][1];
                    if (ea < 0) break;
                    float ax, ay, bx, by;
                    edgePt(ea, ax, ay);
                    edgePt(eb, bx, by);
                    drawLine(pixels, pitch, W, H,
                             (int)std::lround(ax), (int)std::lround(ay),
                             (int)std::lround(bx), (int)std::lround(by),
                             contourCol);
                }
            }
        }
    }

    // Wind quivers
    if (ugrd.nx > 0 && vgrd.nx > 0) {
        uint32_t arrowCol = SDL_MapRGBA(surf->format, 255, 255, 255, 140);
        const int step = 25;

        for (int ay = step / 2; ay < H; ay += step) {
            for (int ax = step / 2; ax < W; ax += step) {
                int gx = std::clamp(ax * ugrd.nx / W, 0, ugrd.nx - 1);
                int gy = std::clamp(ay * ugrd.ny / H, 0, ugrd.ny - 1);

                float u = ugrd.values[gy * ugrd.nx + gx];
                float v = vgrd.values[gy * vgrd.nx + gx];
                float speed = std::sqrt(u * u + v * v);
                if (speed < 0.5f) continue;

                float len = std::clamp(speed * 1.2f, 2.0f, 18.0f);
                // V positive = northward = screen y decreasing (negate)
                float dx = u / speed * len;
                float dy = -v / speed * len;

                int x1 = ax + (int)std::lround(dx);
                int y1 = ay + (int)std::lround(dy);
                drawLine(pixels, pitch, W, H, ax, ay, x1, y1, arrowCol);

                // Arrowhead: two lines at ±30° from shaft direction
                float headLen = std::max(3.0f, len * 0.35f);
                float angle   = std::atan2(dy, dx);
                for (int sign : {-1, 1}) {
                    float ha = angle + sign * 0.5236f; // ±30°
                    int hx = x1 - (int)std::lround(std::cos(ha) * headLen);
                    int hy = y1 - (int)std::lround(std::sin(ha) * headLen);
                    drawLine(pixels, pitch, W, H, x1, y1, hx, hy, arrowCol);
                }
            }
        }
    }

    return surf;
}

// ---------------------------------------------------------------------------
// GFS cycle URL construction
// ---------------------------------------------------------------------------

std::string WxMbProvider::buildNomadsUrl() {
    // Use UTC time 4 hours ago, rounded down to nearest 6h GFS cycle boundary.
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
        hh,
        gmt.tm_year + 1900, gmt.tm_mon + 1, gmt.tm_mday,
        hh);
    return buf;
}

// ---------------------------------------------------------------------------
// WxMbProvider public interface
// ---------------------------------------------------------------------------

WxMbProvider::WxMbProvider(NetworkManager& net) : net_(net) {}

WxMbProvider::~WxMbProvider() {
    if (pendingSurface_) SDL_FreeSurface(pendingSurface_);
    if (texture_)        SDL_DestroyTexture(texture_);
}

void WxMbProvider::update() {
    std::string url = buildNomadsUrl();
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (url == lastUrl_) return; // same GFS cycle already fetched
    }

    LOG_I("WxMb", "Fetching GFS WX subset: {}", url);
    net_.fetchAsync(url, [this, url](std::string rawData) {
        if (rawData.empty()) {
            LOG_W("WxMb", "GFS GRIB2 fetch returned empty response");
            return;
        }
        WorkerService::getInstance().submitTask(
            [this, url, rawData = std::move(rawData)]() {
                std::vector<uint8_t> bytes(rawData.begin(), rawData.end());

                GribField prmsl, ugrd, vgrd;
                if (!decodeGFS(bytes, prmsl, ugrd, vgrd)) {
                    LOG_W("WxMb", "GRIB2 decode failed — non-simple packing or parse error");
                    return;
                }
                LOG_I("WxMb", "GFS decoded: {}pt PRMSL, {}pt UGRD",
                      prmsl.values.size(), ugrd.values.size());

                SDL_Surface* surf = renderToSurface(prmsl, ugrd, vgrd, 660, 330);
                if (!surf) {
                    LOG_W("WxMb", "renderToSurface failed");
                    return;
                }

                std::lock_guard<std::mutex> lk(mutex_);
                if (pendingSurface_) SDL_FreeSurface(pendingSurface_);
                pendingSurface_ = surf;
                dirty_          = true;
                hasData_        = true;
                lastUrl_        = url;
                lastUpdateMs_   = (uint64_t)SDL_GetTicks();
            });
    }, 0); // TTL=0: WxMbProvider tracks cycle freshness via URL comparison
}

SDL_Texture* WxMbProvider::getTexture(SDL_Renderer* renderer, int w, int h) {
    std::lock_guard<std::mutex> lk(mutex_);

    // Return cached texture if still valid and same size
    if (!dirty_ && texture_ && texW_ == w && texH_ == h)
        return texture_;

    if (!pendingSurface_)
        return texture_; // no new data yet

    SDL_Surface* surf  = pendingSurface_;
    pendingSurface_    = nullptr;
    dirty_             = false;

    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    texture_ = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (texture_) {
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_BLEND);
        texW_ = w;
        texH_ = h;
    }
    return texture_;
}

bool WxMbProvider::hasData() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return hasData_;
}

uint64_t WxMbProvider::getLastUpdateMs() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return lastUpdateMs_;
}
