#include "MapWidget.h"
#include "../core/Astronomy.h"
#include "../core/LiveSpotData.h"
#include "../core/Logger.h"
#include "EmbeddedIcons.h"
#include "RenderUtils.h"

#include <algorithm>

#include <SDL_video.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

static constexpr const char *MAP_KEY = "earth_map";
static constexpr const char *NIGHT_MAP_KEY = "night_map";
static constexpr const char *SAT_ICON_KEY = "sat_icon";
static constexpr const char *LINE_AA_KEY = "line_aa";
static constexpr int FALLBACK_W = 1024;
static constexpr int FALLBACK_H = 512;

MapWidget::MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
                     FontManager &fontMgr, NetworkManager &netMgr,
                     std::shared_ptr<HamClockState> state,
                     const AppConfig &config)
    : Widget(x, y, w, h), texMgr_(texMgr), fontMgr_(fontMgr), netMgr_(netMgr),
      state_(std::move(state)), config_(config) {

  const char *driver = SDL_GetCurrentVideoDriver();
  LOG_I("MapWidget", "SDL Video Driver: {}", driver ? driver : "unknown");

  // KMSDRM driver on RPi has issues with SDL_RenderGeometry.
  if (driver && strcasecmp(driver, "kmsdrm") == 0) {
    useCompatibilityRenderPath_ = true;
    LOG_I("MapWidget",
          "KMSDRM detected, enabling night overlay compatibility path.");
  }

  // Initialize map rectangle
  recalcMapRect();
}

MapWidget::~MapWidget() {
  if (nightOverlayTexture_) {
    SDL_DestroyTexture(nightOverlayTexture_);
    nightOverlayTexture_ = nullptr;
  }
}

void MapWidget::recalcMapRect() {
  int mapW = width_;
  int mapH = mapW / 2;
  if (mapH > height_) {
    mapH = height_;
    mapW = mapH * 2;
  }
  mapRect_.x = x_ + (width_ - mapW) / 2;
  mapRect_.y = y_ + (height_ - mapH) / 2;
  mapRect_.w = mapW;
  mapRect_.h = mapH;
}

SDL_FPoint MapWidget::latLonToScreen(double lat, double lon) const {
  double nx = (lon + 180.0) / 360.0;
  double ny = (90.0 - lat) / 180.0;
  float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
  float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
  return {px, py};
}

bool MapWidget::screenToLatLon(int sx, int sy, double &lat, double &lon) const {
  if (sx < mapRect_.x || sx >= mapRect_.x + mapRect_.w || sy < mapRect_.y ||
      sy >= mapRect_.y + mapRect_.h)
    return false;
  double nx = static_cast<double>(sx - mapRect_.x) / mapRect_.w;
  double ny = static_cast<double>(sy - mapRect_.y) / mapRect_.h;
  lon = nx * 360.0 - 180.0;
  lat = 90.0 - ny * 180.0;
  return true;
}

static const char *kMonthNames[] = {
    "january", "february", "march",     "april",   "may",      "june",
    "july",    "august",   "september", "october", "november", "december"};

void MapWidget::update() {
  uint32_t nowMs = SDL_GetTicks();
  if (nowMs - lastPosUpdateMs_ < 1000) // 1 second is plenty
    return;
  lastPosUpdateMs_ = nowMs;

  auto now = std::chrono::system_clock::now();
  auto sun = Astronomy::sunPosition(now);
  sunLat_ = sun.lat;
  sunLon_ = sun.lon;

  // Cache Great Circle

  if (state_->dxActive) {
    if (state_->deLocation.lat != lastDE_.lat ||
        state_->deLocation.lon != lastDE_.lon ||
        state_->dxLocation.lat != lastDX_.lat ||
        state_->dxLocation.lon != lastDX_.lon) {
      cachedGreatCircle_ = Astronomy::calculateGreatCirclePath(
          state_->deLocation, state_->dxLocation, 250);
      lastDE_ = state_->deLocation;
      lastDX_ = state_->dxLocation;
    }
  } else {
    cachedGreatCircle_.clear();
  }

  // Check if month changed
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm *tm = std::localtime(&t);
  int month = tm->tm_mon + 1; // 1-12

  if (month != currentMonth_) {
    currentMonth_ = month;
    char url[256];
    std::snprintf(url, sizeof(url),
                  "https://assets.science.nasa.gov/content/dam/science/esd/eo/"
                  "images/bmng/bmng-base/%s/world.2004%02d.3x5400x2700.jpg",
                  kMonthNames[month - 1], month);

    LOG_I("MapWidget", "Starting async fetch for {}", url);
    netMgr_.fetchAsync(
        url,
        [this, url_str = std::string(url)](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for {}", data.size(),
                  url_str.size() > 50 ? "NASA Map" : url_str);
            std::lock_guard<std::mutex> lock(mapDataMutex_);
            pendingMapData_ = std::move(data);
          } else {
            LOG_E("MapWidget", "Fetch failed or empty for {}", url_str);
          }
        },
        86400 * 30); // Cache for a month

    // Also fetch night lights map (NASA Black Marble 2012)
    const char *nightUrl = "https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                           "79000/79765/dnb_land_ocean_ice.2012.3600x1800.jpg";
    LOG_I("MapWidget", "Starting async fetch for Night Lights");
    netMgr_.fetchAsync(
        nightUrl,
        [this](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for Night Lights",
                  data.size());
            std::lock_guard<std::mutex> lock(mapDataMutex_);
            pendingNightMapData_ = std::move(data);
          }
        },
        86400 * 365); // Cache for a year
  }
}

bool MapWidget::onMouseUp(int mx, int my, Uint16 mod) {
  double lat, lon;
  if (!screenToLatLon(mx, my, lat, lon))
    return false;

  if (mod & KMOD_SHIFT) {
    // Shift-click: set DE (current location)
    state_->deLocation = {lat, lon};
    state_->deGrid = Astronomy::latLonToGrid(lat, lon);
  } else {
    // Normal click: set DX (target)
    state_->dxLocation = {lat, lon};
    state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
    state_->dxActive = true;
  }

  return true;
}

void MapWidget::onMouseMove(int mx, int my) {
  double lat, lon;
  if (!screenToLatLon(mx, my, lat, lon)) {
    tooltip_.visible = false;
    return;
  }

  // Helper: distance in screen pixels between cursor and a lat/lon point
  auto screenDist = [&](double plat, double plon) -> float {
    SDL_FPoint pt = latLonToScreen(plat, plon);
    float dx = pt.x - mx;
    float dy = pt.y - my;
    return std::sqrt(dx * dx + dy * dy);
  };

  constexpr float kHitRadius = 10.0f;
  std::string tip;

  // 1. Check DE marker
  if (screenDist(state_->deLocation.lat, state_->deLocation.lon) < kHitRadius) {
    tip = "DE: " + (state_->deCallsign.empty() ? "Home" : state_->deCallsign);
    tip += " [" + state_->deGrid + "]";
  }

  // 2. Check DX marker
  if (tip.empty() && state_->dxActive &&
      screenDist(state_->dxLocation.lat, state_->dxLocation.lon) < kHitRadius) {
    tip = "DX [" + state_->dxGrid + "]";
    char buf[64];
    std::snprintf(buf, sizeof(buf), " %.1f°N %.1f°%c",
                  std::fabs(state_->dxLocation.lat),
                  std::fabs(state_->dxLocation.lon),
                  state_->dxLocation.lon >= 0 ? 'E' : 'W');
    tip += buf;
  }

  // 3. Check sun marker
  if (tip.empty() && screenDist(sunLat_, sunLon_) < kHitRadius) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Sun: %.1f°N %.1f°%c", std::fabs(sunLat_),
                  std::fabs(sunLon_), sunLon_ >= 0 ? 'E' : 'W');
    tip = buf;
  }

  // 4. Check satellite
  if (tip.empty() && predictor_ && predictor_->isReady()) {
    SubSatPoint ssp = predictor_->subSatPoint();
    if (screenDist(ssp.lat, ssp.lon) < kHitRadius + 4) {
      tip = predictor_->satName();
      char buf[64];
      std::snprintf(buf, sizeof(buf), " Alt:%.0fkm", ssp.altitude);
      tip += buf;
    }
  }

  // 5. Check DX Cluster spots
  if (tip.empty() && dxcStore_) {
    auto data = dxcStore_->get();
    for (const auto &spot : data.spots) {
      if (spot.txLat == 0.0 && spot.txLon == 0.0)
        continue;
      if (screenDist(spot.txLat, spot.txLon) < kHitRadius) {
        tip = spot.txCall;
        char buf[64];
        std::snprintf(buf, sizeof(buf), " %.1f kHz", spot.freqKhz);
        tip += buf;
        int bi = freqToBandIndex(spot.freqKhz);
        if (bi >= 0)
          tip += std::string(" (") + kBands[bi].name + ")";
        if (!spot.mode.empty())
          tip += " " + spot.mode;
        break;
      }
    }
  }

  // 6. Fallback: show lat/lon under cursor
  if (tip.empty()) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f°%c %.2f°%c  %s", std::fabs(lat),
                  lat >= 0 ? 'N' : 'S', std::fabs(lon), lon >= 0 ? 'E' : 'W',
                  Astronomy::latLonToGrid(lat, lon).c_str());
    tip = buf;
  }

  tooltip_.text = tip;
  tooltip_.x = mx;
  tooltip_.y = my;
  tooltip_.visible = true;
  tooltip_.timestamp = SDL_GetTicks();
}

void MapWidget::renderMarker(SDL_Renderer *renderer, double lat, double lon,
                             Uint8 r, Uint8 g, Uint8 b, MarkerShape shape,
                             bool outline) {
  SDL_FPoint pt = latLonToScreen(lat, lon);
  float radius = 3.0f;

  if (shape == MarkerShape::Circle && r == 255 && g == 255 && b == 0) {
    radius = std::max(4.0f, std::min(mapRect_.w, mapRect_.h) / 60.0f);
  } else if (shape == MarkerShape::Circle) {
    radius = std::max(3.0f, std::min(mapRect_.w, mapRect_.h) / 80.0f);
  } else {
    radius = 2.0f;
  }

  SDL_Texture *tex = texMgr_.get(
      shape == MarkerShape::Circle ? "marker_circle" : "marker_square");
  if (tex) {
    if (outline) {
      // Draw a slightly larger black version as outline
      float oRad = radius + 1.0f;
      SDL_FRect oDst = {pt.x - oRad, pt.y - oRad, oRad * 2, oRad * 2};
      SDL_SetTextureColorMod(tex, 0, 0, 0);
      SDL_SetTextureAlphaMod(tex, 255);
      SDL_RenderCopyF(renderer, tex, nullptr, &oDst);
    }

    SDL_FRect dst = {pt.x - radius, pt.y - radius, radius * 2, radius * 2};
    SDL_SetTextureColorMod(tex, r, g, b);
    SDL_SetTextureAlphaMod(tex, 255);
    SDL_RenderCopyF(renderer, tex, nullptr, &dst);
  }
}

void MapWidget::renderGreatCircle(SDL_Renderer *renderer) {
  if (!state_->dxActive || cachedGreatCircle_.empty())
    return;

  auto &path = cachedGreatCircle_;

  SDL_Color color = {255, 255, 0, 255}; // Yellow
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  std::vector<SDL_FPoint> points;
  for (const auto &ll : path) {
    points.push_back(latLonToScreen(ll.lat, ll.lon));
  }

  std::vector<SDL_FPoint> segment;
  for (size_t i = 0; i < points.size(); ++i) {
    if (i > 0) {
      float lon1 = path[i - 1].lon;
      float lon2 = path[i].lon;
      if (std::fabs(lon1 - lon2) > 180.0) {
        if (segment.size() >= 2) {
          RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                            static_cast<int>(segment.size()),
                                            1.2f, color);
        }
        segment.clear();
      }
    }
    segment.push_back(points[i]);
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                      static_cast<int>(segment.size()), 1.2f,
                                      color);
  }
}

void MapWidget::renderNightOverlay(SDL_Renderer *renderer) {
  const float sLatRad = sunLat_ * M_PI / 180.0;
  const float sLonRad = sunLon_ * M_PI / 180.0;
  const float sinSLat = std::sin(sLatRad);
  const float cosSLat = std::cos(sLatRad);

  constexpr int gridW = 80;
  constexpr int gridH = 48;
  const float stepX = static_cast<float>(mapRect_.w) / gridW;
  const float stepY = static_cast<float>(mapRect_.h) / gridH;

  // Constants matching original HamClock: 12 deg grayline, 0.75 power curve
  constexpr float GRAYLINE_COS = -0.21f; // ~cos(90+12)
  constexpr float GRAYLINE_POW = 0.8f;   // Slightly steeper for deeper night

  SDL_Rect clip = mapRect_;
  SDL_RenderSetClipRect(renderer, &clip);

  // Ensure robust geometry helper textures exist
  texMgr_.generateWhiteTexture(renderer);
  texMgr_.generateBlackTexture(renderer);

#if SDL_VERSION_ATLEAST(2, 0, 18)
  // -------------------------------------------------------------------------
  // High-Fidelity Path (SDL 2.0.18+)
  // -------------------------------------------------------------------------

  // Force blend mode for geometry shading
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  std::vector<SDL_Vertex> shadowVerts;
  std::vector<SDL_Vertex> lightVerts;
  shadowVerts.reserve((gridW + 1) * (gridH + 1));
  lightVerts.reserve((gridW + 1) * (gridH + 1));

  for (int j = 0; j <= gridH; ++j) {
    float sy = mapRect_.y + j * stepY;
    for (int i = 0; i <= gridW; ++i) {
      float sx = mapRect_.x + i * stepX;
      double lat, lon;
      if (screenToLatLon((int)sx, (int)sy, lat, lon)) {
        double latRad = lat * M_PI / 180.0;
        double dLonRad = (lon * M_PI / 180.0) - sLonRad;
        double cosZ = sinSLat * std::sin(latRad) +
                      cosSLat * std::cos(latRad) * std::cos(dLonRad);
        float fd =
            (cosZ > 0)
                ? 1.0f
                : (cosZ > GRAYLINE_COS
                       ? 1.0f - std::pow(cosZ / GRAYLINE_COS, GRAYLINE_POW)
                       : 0.0f);
        float nf = 1.0f - fd;
        float u = (float)i / gridW;
        float v = (float)j / gridH;
        // Optimization for Red-tint issues:
        // Use a BLACK 1x1 texture and WHITE vertex colors.
        // This forces the hardware to use Black * Alpha instead of
        // possibly misinterpreting 0,0,0,Alpha as a color key or byte-swapped
        // Red.
        shadowVerts.push_back(
            {{sx, sy}, {255, 255, 255, (Uint8)(nf * 255)}, {0, 0}});
        lightVerts.push_back(
            {{sx, sy}, {255, 255, 255, (Uint8)(nf * 255)}, {u, v}});
      } else {
        shadowVerts.push_back({{sx, sy}, {0, 0, 0, 0}, {0, 0}});
        lightVerts.push_back({{sx, sy}, {0, 0, 0, 0}, {0, 0}});
      }
    }
  }

  std::vector<int> indices;
  indices.reserve(gridW * gridH * 6);
  for (int j = 0; j < gridH; ++j) {
    for (int i = 0; i < gridW; ++i) {
      int p0 = j * (gridW + 1) + i;
      int p1 = p0 + 1;
      int p2 = (j + 1) * (gridW + 1) + i;
      int p3 = p2 + 1;
      indices.push_back(p0);
      indices.push_back(p1);
      indices.push_back(p2);
      indices.push_back(p2);
      indices.push_back(p1);
      indices.push_back(p3);
    }
  }

  // Draw shaded overlay using a BLACK texture and WHITE vertex colors
  // This is the most bulletproof way to get alpha-blended black shading.
  SDL_RenderGeometry(renderer, texMgr_.get("black"), shadowVerts.data(),
                     (int)shadowVerts.size(), indices.data(),
                     (int)indices.size());
  if (config_.mapNightLights) {
    SDL_Texture *nightTex = texMgr_.get(NIGHT_MAP_KEY);
    if (nightTex) {
      SDL_SetTextureColorMod(nightTex, 255, 255, 255);
      SDL_SetTextureBlendMode(nightTex, SDL_BLENDMODE_BLEND);
      SDL_RenderGeometry(renderer, nightTex, lightVerts.data(),
                         (int)lightVerts.size(), indices.data(),
                         (int)indices.size());
    }
  }
#else
  // -------------------------------------------------------------------------
  // Compatibility Path (SDL < 2.0.18)
  // -------------------------------------------------------------------------
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  // 1. Draw shading
  for (int j = 0; j < gridH; ++j) {
    for (int i = 0; i < gridW; ++i) {
      float sx = mapRect_.x + i * stepX;
      float sy = mapRect_.y + j * stepY;
      double lat, lon;
      if (screenToLatLon((int)(sx + stepX * 0.5f), (int)(sy + stepY * 0.5f),
                         lat, lon)) {
        double latRad = lat * M_PI / 180.0;
        double dLonRad = (lon * M_PI / 180.0) - sLonRad;
        double cosZ = sinSLat * std::sin(latRad) +
                      cosSLat * std::cos(latRad) * std::cos(dLonRad);
        float fd =
            (cosZ > 0)
                ? 1.0f
                : (cosZ > GRAYLINE_COS
                       ? 1.0f - std::pow(cosZ / GRAYLINE_COS, GRAYLINE_POW)
                       : 0.0f);
        float darkness = 1.0f - fd;
        if (darkness > 0) {
          SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(darkness * 255));
          SDL_Rect r = {(int)sx, (int)sy,
                        (int)(mapRect_.x + (i + 1) * stepX) - (int)sx,
                        (int)(mapRect_.y + (j + 1) * stepY) - (int)sy};
          SDL_RenderFillRect(renderer, &r);
        }
      }
    }
  }

  // 2. Draw night lights
  if (config_.mapNightLights) {
    SDL_Texture *nightTex = texMgr_.get(NIGHT_MAP_KEY);
    if (nightTex) {
      SDL_SetTextureColorMod(nightTex, 255, 255, 255);
      SDL_SetTextureBlendMode(nightTex, SDL_BLENDMODE_BLEND);

      int tW, tH;
      SDL_QueryTexture(nightTex, nullptr, nullptr, &tW, &tH);
      float srcStepX = (float)tW / gridW;
      float srcStepY = (float)tH / gridH;

      for (int j = 0; j < gridH; ++j) {
        float sy = mapRect_.y + j * stepY;
        for (int i = 0; i < gridW; ++i) {
          float sx = mapRect_.x + i * stepX;

          // Darkness factor at center of cell
          float fd = 1.0f;
          double lat, lon;
          if (screenToLatLon((int)(sx + stepX * 0.5f), (int)(sy + stepY * 0.5f),
                             lat, lon)) {
            double latRad = lat * M_PI / 180.0;
            double dLonRad = (lon * M_PI / 180.0) - sLonRad;
            double cosZ = sinSLat * std::sin(latRad) +
                          cosSLat * std::cos(latRad) * std::cos(dLonRad);
            fd = (cosZ > 0)
                     ? 1.0f
                     : (cosZ > GRAYLINE_COS
                            ? 1.0f - std::pow(cosZ / GRAYLINE_COS, GRAYLINE_POW)
                            : 0.0f);
          }

          float darkness = 1.0f - fd;
          if (darkness > 0.05f) {
            SDL_SetTextureAlphaMod(nightTex, (Uint8)(darkness * 255));
            SDL_Rect src = {(int)(i * srcStepX), (int)(j * srcStepY),
                            (int)((i + 1) * srcStepX) - (int)(i * srcStepX),
                            (int)((j + 1) * srcStepY) - (int)(j * srcStepY)};

            SDL_Rect dst = {(int)sx, (int)sy,
                            (int)(mapRect_.x + (i + 1) * stepX) - (int)sx,
                            (int)(mapRect_.y + (j + 1) * stepY) - (int)sy};

            SDL_RenderCopy(renderer, nightTex, &src, &dst);
          }
        }
      }
    } else {
      static uint32_t lastLog = 0;
      if (SDL_GetTicks() - lastLog > 10000) {
        LOG_W("MapWidget", "Night Lights texture not yet loaded");
        lastLog = SDL_GetTicks();
      }
    }
  }
#endif
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::render(SDL_Renderer *renderer) {

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  // Check for any newly downloaded map data from background thread
  {
    std::lock_guard<std::mutex> lock(mapDataMutex_);
    if (!pendingMapData_.empty()) {
      texMgr_.loadFromMemory(renderer, MAP_KEY, pendingMapData_);
      SDL_Texture *t = texMgr_.get(MAP_KEY);
      if (t)
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
      pendingMapData_.clear();
    }
    if (!pendingNightMapData_.empty()) {
      texMgr_.loadFromMemory(renderer, NIGHT_MAP_KEY, pendingNightMapData_);
      pendingNightMapData_.clear();
    }
  }

  if (!mapLoaded_) {
    SDL_Texture *tex = texMgr_.get(MAP_KEY);
    if (!tex) {
      tex = texMgr_.generateEarthFallback(renderer, MAP_KEY, FALLBACK_W,
                                          FALLBACK_H);
    }
    // Load embedded satellite icon
    texMgr_.loadFromMemory(renderer, SAT_ICON_KEY, assets_satellite_png,
                           assets_satellite_png_len);
    texMgr_.generateLineTexture(renderer, LINE_AA_KEY);
    texMgr_.generateMarkerTextures(renderer);

    // Force base map to be opaque
    SDL_Texture *t = texMgr_.get(MAP_KEY);
    if (t) {
      SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }
    mapLoaded_ = true;
  }

  SDL_Texture *mapTex = texMgr_.get(MAP_KEY);
  if (mapTex)
    SDL_RenderCopy(renderer, mapTex, nullptr, &mapRect_);

  renderNightOverlay(renderer);
  renderGreatCircle(renderer);

  renderMarker(renderer, state_->deLocation.lat, state_->deLocation.lon, 255,
               165, 0);
  if (state_->dxActive) {
    renderMarker(renderer, state_->dxLocation.lat, state_->dxLocation.lon, 0,
                 255, 0);
  }

  renderAuroraOverlay(renderer);
  renderSatellite(renderer);
  renderSpotOverlay(renderer);
  renderDXClusterSpots(renderer);
  renderMarker(renderer, sunLat_, sunLon_, 255, 255, 0, MarkerShape::Circle,
               true);

  renderTooltip(renderer);

  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);
}

void MapWidget::renderSatellite(SDL_Renderer *renderer) {
  if (!predictor_ || !predictor_->isReady())
    return;
  SubSatPoint ssp = predictor_->subSatPoint();
  renderSatFootprint(renderer, ssp.lat, ssp.lon, ssp.footprint);
  renderSatGroundTrack(renderer);

  SDL_FPoint pt = latLonToScreen(ssp.lat, ssp.lon);
  int iconSz = std::max(16, std::min(mapRect_.w, mapRect_.h) / 25);
  SDL_Texture *satTex = texMgr_.get(SAT_ICON_KEY);
  if (satTex) {
    SDL_FRect dst = {pt.x - iconSz / 2.0f, pt.y - iconSz / 2.0f,
                     static_cast<float>(iconSz), static_cast<float>(iconSz)};
    SDL_RenderCopyF(renderer, satTex, nullptr, &dst);
  }
}

void MapWidget::renderSatFootprint(SDL_Renderer *renderer, double lat,
                                   double lon, double footprintKm) {
  if (footprintKm <= 0.0)
    return;
  constexpr double kKmPerDeg = 111.32;
  double angRadDeg = (footprintKm / 2.0) / kKmPerDeg;
  double latRad = lat * M_PI / 180.0;
  double cosLat = std::cos(latRad);
  if (std::fabs(cosLat) < 0.01)
    cosLat = 0.01;

  constexpr int kSegments = 72;
  SDL_RenderSetClipRect(renderer, &mapRect_);
  std::vector<SDL_FPoint> segment;
  SDL_FPoint prev{};
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  for (int i = 0; i <= kSegments; ++i) {
    double theta = 2.0 * M_PI * i / kSegments;
    double pLat = lat + angRadDeg * std::cos(theta);
    double pLon = lon + angRadDeg * std::sin(theta) / cosLat;
    while (pLon > 180.0)
      pLon -= 360.0;
    while (pLon < -180.0)
      pLon += 360.0;

    SDL_FPoint cur = latLonToScreen(pLat, pLon);
    if (i > 0 && std::abs(cur.x - prev.x) > mapRect_.w / 2.0f) {
      if (segment.size() >= 2) {
        RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                          static_cast<int>(segment.size()),
                                          2.0f, {255, 255, 0, 120});
      }
      segment.clear();
    }
    segment.push_back(cur);
    prev = cur;
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                      static_cast<int>(segment.size()), 2.0f,
                                      {255, 255, 0, 120});
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSatGroundTrack(SDL_Renderer *renderer) {
  if (!predictor_)
    return;
  std::time_t now = std::time(nullptr);
  auto track = predictor_->groundTrack(now, 90, 30);
  if (track.size() < 2)
    return;
  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  for (size_t i = 1; i < track.size(); ++i) {
    if (std::fabs(track[i].lon - track[i - 1].lon) > 180.0)
      continue;
    SDL_FPoint p1 = latLonToScreen(track[i - 1].lat, track[i - 1].lon);
    SDL_FPoint p2 = latLonToScreen(track[i].lat, track[i].lon);
    RenderUtils::drawThickLineTextured(renderer, lineTex, p1.x, p1.y, p2.x,
                                       p2.y, 2.0f, {255, 200, 0, 150});
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSpotOverlay(SDL_Renderer *renderer) {
  if (!spotStore_)
    return;
  auto data = spotStore_->get();
  if (!data.valid || data.spots.empty())
    return;
  bool anySelected = false;
  for (int i = 0; i < kNumBands; ++i) {
    if (data.selectedBands[i]) {
      anySelected = true;
      break;
    }
  }
  if (!anySelected)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  LatLon de = state_->deLocation;
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  int renderedCount = 0;
  const int MAX_MAP_SPOTS = 500;

  for (const auto &spot : data.spots) {
    if (renderedCount >= MAX_MAP_SPOTS) {
      static uint32_t lastWarn = 0;
      if (SDL_GetTicks() - lastWarn > 60000) {
        LOG_W(
            "MapWidget",
            "Too many spots ({}). Truncating map display to {} for stability.",
            data.spots.size(), MAX_MAP_SPOTS);
        lastWarn = SDL_GetTicks();
      }
      break;
    }

    int bandIdx = freqToBandIndex(spot.freqKhz);
    if (bandIdx < 0 || !data.selectedBands[bandIdx])
      continue;
    double lat, lon;
    if (!Astronomy::gridToLatLon(spot.receiverGrid, lat, lon))
      continue;

    renderedCount++;
    const auto &bc = kBands[bandIdx].color;
    // Reduce segments to 30 for performance; 100 is overkill for small map
    // lines.
    auto path = Astronomy::calculateGreatCirclePath(de, {lat, lon}, 30);
    SDL_Color color = {bc.r, bc.g, bc.b, 180};
    std::vector<SDL_FPoint> segment;
    for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0 && std::fabs(path[i].lon - path[i - 1].lon) > 180.0) {
        if (segment.size() >= 2) {
          RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                            static_cast<int>(segment.size()),
                                            1.5f, color);
        }
        segment.clear();
      }
      segment.push_back(latLonToScreen(path[i].lat, path[i].lon));
    }
    if (segment.size() >= 2) {
      RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                        static_cast<int>(segment.size()), 1.5f,
                                        color);
    }
    renderMarker(renderer, lat, lon, bc.r, bc.g, bc.b, MarkerShape::Square,
                 true);
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderDXClusterSpots(SDL_Renderer *renderer) {
  if (!dxcStore_)
    return;
  auto data = dxcStore_->get();
  if (data.spots.empty())
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  // Filter spots to render
  std::vector<DXClusterSpot> spotsToRender;
  if (data.hasSelection) {
    spotsToRender.push_back(data.selectedSpot);
  } else {
    // Default: Show None
    // If user wanted Show All, we'd copy all.
    // But user asked: "NOT plot all spots on the map and only plot those
    // clicked on" So default is empty.
  }

  for (const auto &spot : spotsToRender) {
    if (spot.txLat == 0.0 && spot.txLon == 0.0)
      continue;

    // Determine color based on band
    SDL_Color color = {255, 255, 255, 255}; // Default white
    int bandIdx = freqToBandIndex(spot.freqKhz);
    if (bandIdx >= 0) {
      color = kBands[bandIdx].color;
    }

    // Draw path if RX location is known and different from TX
    if ((spot.rxLat != 0.0 || spot.rxLon != 0.0) &&
        (std::abs(spot.txLat - spot.rxLat) > 0.01 ||
         std::abs(spot.txLon - spot.rxLon) > 0.01)) {

      auto path = Astronomy::calculateGreatCirclePath(
          {spot.rxLat, spot.rxLon}, {spot.txLat, spot.txLon}, 50);

      std::vector<SDL_FPoint> segment;
      SDL_Color lineColor = {color.r, color.g, color.b, 100};

      for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0 && std::fabs(path[i].lon - path[i - 1].lon) > 180.0) {
          if (segment.size() >= 2) {
            RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                              static_cast<int>(segment.size()),
                                              1.0f, lineColor);
          }
          segment.clear();
        }
        segment.push_back(latLonToScreen(path[i].lat, path[i].lon));
      }
      if (segment.size() >= 2) {
        RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                          static_cast<int>(segment.size()),
                                          1.0f, lineColor);
      }
    }

    // Plot transmitter as a small circle with band color
    renderMarker(renderer, spot.txLat, spot.txLon, color.r, color.g, color.b,
                 MarkerShape::Circle, true);
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcMapRect();
  if (nightOverlayTexture_) {
    SDL_DestroyTexture(nightOverlayTexture_);
    nightOverlayTexture_ = nullptr;
  }
}

// --- Tooltip Rendering ---

void MapWidget::renderTooltip(SDL_Renderer *renderer) {
  if (!tooltip_.visible || tooltip_.text.empty())
    return;

  // Fade out after 3 seconds of no motion
  uint32_t age = SDL_GetTicks() - tooltip_.timestamp;
  if (age > 3000) {
    tooltip_.visible = false;
    return;
  }

  int tw = 0, th = 0;
  int ptSize = std::max(9, height_ / 40);
  SDL_Texture *textTex = fontMgr_.renderText(
      renderer, tooltip_.text, {255, 255, 255, 255}, ptSize, &tw, &th);
  if (!textTex)
    return;

  int padX = 6, padY = 3;
  int boxW = tw + padX * 2;
  int boxH = th + padY * 2;

  // Position: offset above cursor, clamped to widget bounds
  int bx = tooltip_.x - boxW / 2;
  int by = tooltip_.y - boxH - 12;
  if (bx < x_)
    bx = x_;
  if (bx + boxW > x_ + width_)
    bx = x_ + width_ - boxW;
  if (by < y_)
    by = tooltip_.y + 16; // flip below cursor

  // Background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 210);
  SDL_Rect bg = {bx, by, boxW, boxH};
  SDL_RenderFillRect(renderer, &bg);

  // Border
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 200);
  SDL_RenderDrawRect(renderer, &bg);

  // Text
  SDL_Rect dst = {bx + padX, by + padY, tw, th};
  SDL_RenderCopy(renderer, textTex, nullptr, &dst);
  SDL_DestroyTexture(textTex);
}

// --- Semantic API ---

std::string MapWidget::getName() const { return "Map"; }

std::vector<std::string> MapWidget::getActions() const {
  std::vector<std::string> actions;
  actions.push_back("set_de");
  if (state_->dxActive) {
    actions.push_back("set_dx");
  }
  return actions;
}

SDL_Rect MapWidget::getActionRect(const std::string &action) const {
  if (action == "set_de") {
    SDL_FPoint pt =
        latLonToScreen(state_->deLocation.lat, state_->deLocation.lon);
    return {static_cast<int>(pt.x) - 10, static_cast<int>(pt.y) - 10, 20, 20};
  }
  if (action == "set_dx" && state_->dxActive) {
    SDL_FPoint pt =
        latLonToScreen(state_->dxLocation.lat, state_->dxLocation.lon);
    return {static_cast<int>(pt.x) - 10, static_cast<int>(pt.y) - 10, 20, 20};
  }
  return {0, 0, 0, 0};
}

nlohmann::json MapWidget::getDebugData() const {
  nlohmann::json j;
  j["projection"] = "Equirectangular";

  // DE/DX positions
  j["de"] = {{"lat", state_->deLocation.lat},
             {"lon", state_->deLocation.lon},
             {"grid", state_->deGrid}};
  j["dx_active"] = state_->dxActive;
  if (state_->dxActive) {
    j["dx"] = {{"lat", state_->dxLocation.lat},
               {"lon", state_->dxLocation.lon},
               {"grid", state_->dxGrid}};
    // Calculate distance and bearing
    double dist =
        Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
    double brg =
        Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
    j["dx"]["distance_km"] = static_cast<int>(dist);
    j["dx"]["bearing"] = static_cast<int>(brg);
  }

  // Sun
  j["sun"] = {{"lat", sunLat_}, {"lon", sunLon_}};

  // Satellite
  if (predictor_ && predictor_->isReady()) {
    SubSatPoint ssp = predictor_->subSatPoint();
    j["satellite"] = {{"name", predictor_->satName()},
                      {"lat", ssp.lat},
                      {"lon", ssp.lon},
                      {"alt_km", ssp.altitude}};
  }

  // Spot counts
  if (spotStore_) {
    auto sd = spotStore_->get();
    j["live_spot_count"] = static_cast<int>(sd.spots.size());
  }
  if (dxcStore_) {
    auto dd = dxcStore_->get();
    j["dxc_spot_count"] = static_cast<int>(dd.spots.size());
    j["dxc_connected"] = dd.connected;
  }

  // Tooltip
  if (tooltip_.visible) {
    j["tooltip"] = tooltip_.text;
  }

  return j;
}

void MapWidget::renderAuroraOverlay(SDL_Renderer *renderer) {
  if (!auroraStore_)
    return;

  // For now, we'll fetch the JSON data directly from NOAA
  // In a production implementation, this should be cached/shared with
  // NOAAProvider
  static std::string cachedAuroraData;
  static uint32_t lastFetchTime = 0;
  uint32_t now = SDL_GetTicks();

  // Fetch every 30 minutes
  if (cachedAuroraData.empty() || (now - lastFetchTime) > 1800000) {
    // For now, skip the fetch and just return
    // TODO: Share the Aurora JSON data from NOAAProvider
    return;
  }

  // Parse the JSON and render green glows
  // Format: "coordinates":[[lon,lat,val],...]
  size_t coords_pos = cachedAuroraData.find("\"coordinates\"");
  if (coords_pos == std::string::npos)
    return;

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  size_t p = coords_pos;
  while ((p = cachedAuroraData.find('[', p)) != std::string::npos) {
    int lon, lat, val;
    if (sscanf(cachedAuroraData.c_str() + p, "[%d,%d,%d]", &lon, &lat, &val) ==
            3 ||
        sscanf(cachedAuroraData.c_str() + p, "[%d, %d, %d]", &lon, &lat,
               &val) == 3) {
      if (val > 0) {
        // Convert lon (0-359) to -180 to 180
        double longitude = lon;
        if (longitude >= 180)
          longitude -= 360;

        // Map to screen coordinates
        SDL_FPoint screenPos = latLonToScreen(lat, longitude);

        // Calculate alpha based on value (0-100)
        Uint8 alpha = static_cast<Uint8>((val * 255) / 100);
        if (alpha > 255)
          alpha = 255;

        // Draw green glow
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, alpha);

        // Draw a small filled circle for the glow
        int radius = 3;
        for (int dy = -radius; dy <= radius; dy++) {
          for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
              SDL_RenderDrawPoint(renderer, static_cast<int>(screenPos.x) + dx,
                                  static_cast<int>(screenPos.y) + dy);
            }
          }
        }
      }
    }
    p++;
  }
}
