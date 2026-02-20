#ifdef _WIN32
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif
#include "MapWidget.h"
#include "../core/Astronomy.h"
#include "../core/Constants.h"
#include "../core/LiveSpotData.h"
#include "../core/Logger.h"
#include "../core/PropEngine.h"
#include "../core/WorkerService.h"
#include "../services/IonosondeProvider.h"
#include "../services/MufRtProvider.h"
#include "EmbeddedIcons.h"
#include "RenderUtils.h"
#include <fmt/core.h>

#include <algorithm>

#include <SDL_video.h>
#include <chrono>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <cmath>
#include <cstring>
#include <vector>

static constexpr const char *MAP_KEY = "earth_map";
static constexpr const char *NIGHT_MAP_KEY = "night_map";
static constexpr const char *SAT_ICON_KEY = "sat_icon";
static constexpr const char *LINE_AA_KEY = "line_aa";
static constexpr int FALLBACK_W = 1024;
static constexpr int FALLBACK_H = 512;

struct RobinsonCoeff {
  float x;
  float y;
};
static const RobinsonCoeff robinson_coeffs[] = {
    {1.0000, 0.0000}, {0.9986, 0.0620}, {0.9954, 0.1240}, {0.9900, 0.1860},
    {0.9822, 0.2480}, {0.9730, 0.3100}, {0.9600, 0.3720}, {0.9427, 0.4340},
    {0.9216, 0.4958}, {0.8962, 0.5571}, {0.8679, 0.6176}, {0.8350, 0.6769},
    {0.7986, 0.7346}, {0.7597, 0.7903}, {0.7186, 0.8435}, {0.6732, 0.8936},
    {0.6213, 0.9394}, {0.5722, 0.9761}, {0.5322, 1.0000}};

static void projectRobinson(double lat, double lon, double &nx, double &ny) {
  double abs_lat = std::abs(lat);
  if (abs_lat > 90.0)
    abs_lat = 90.0;
  int idx = static_cast<int>(abs_lat / 5.0);
  if (idx >= 18)
    idx = 17;
  double remainder = (abs_lat - idx * 5.0) / 5.0;

  double x_coeff =
      robinson_coeffs[idx].x +
      (robinson_coeffs[idx + 1].x - robinson_coeffs[idx].x) * remainder;
  double y_coeff =
      robinson_coeffs[idx].y +
      (robinson_coeffs[idx + 1].y - robinson_coeffs[idx].y) * remainder;

  nx = (lon / 180.0) * x_coeff;        // [-1, 1]
  ny = (lat < 0) ? -y_coeff : y_coeff; // [-1, 1]
}

static void inverseRobinson(double nx, double ny, double &lat, double &lon) {
  double low = -90.0, high = 90.0;
  for (int i = 0; i < 20; ++i) {
    double mid = (low + high) / 2.0;
    double dummy_nx, mid_ny;
    projectRobinson(mid, 0, dummy_nx, mid_ny);
    if (mid_ny < ny)
      low = mid;
    else
      high = mid;
  }
  lat = (low + high) / 2.0;

  double abs_lat = std::abs(lat);
  int idx = static_cast<int>(abs_lat / 5.0);
  if (idx >= 18)
    idx = 17;
  double remainder = (abs_lat - idx * 5.0) / 5.0;
  double x_coeff =
      robinson_coeffs[idx].x +
      (robinson_coeffs[idx + 1].x - robinson_coeffs[idx].x) * remainder;
  if (x_coeff < 0.01)
    x_coeff = 0.01;
  lon = (nx / x_coeff) * 180.0;
  if (lon > 180.0)
    lon = 180.0;
  if (lon < -180.0)
    lon = -180.0;
}

MapWidget::MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
                     FontManager &fontMgr, NetworkManager &netMgr,
                     std::shared_ptr<HamClockState> state, AppConfig &config)
    : Widget(x, y, w, h), texMgr_(texMgr), fontMgr_(fontMgr), netMgr_(netMgr),
      state_(std::move(state)), config_(config) {

  const char *driver = SDL_GetCurrentVideoDriver();
  LOG_D("MapWidget", "SDL Video Driver: {}", driver ? driver : "unknown");

  // KMSDRM driver on RPi has issues with SDL_RenderGeometry.
  if (driver && strcasecmp(driver, "kmsdrm") == 0) {
    useCompatibilityRenderPath_ = true;
    LOG_D("MapWidget",
          "KMSDRM detected, enabling night overlay compatibility path.");
  }

  // Initialize MapViewMenu
  mapViewMenu_ = std::make_unique<MapViewMenu>(fontMgr);
  mapViewMenu_->setTheme(config.theme);

  // Initialize map rectangle
  recalcMapRect();
}

MapWidget::~MapWidget() {
  MemoryMonitor::getInstance().destroyTexture(nightOverlayTexture_);
  MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
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
  if (config_.projection == "robinson") {
    double rnx, rny;
    projectRobinson(lat, lon, rnx, rny);
    float px = static_cast<float>(mapRect_.x + (rnx + 1.0) * 0.5 * mapRect_.w);
    float py = static_cast<float>(mapRect_.y + (1.0 - rny) * 0.5 * mapRect_.h);
    return {px, py};
  }
  if (config_.projection == "mercator") {
    // Standard Mercator clipped to ~85.05 degrees to maintain 2:1 aspect ratio
    constexpr double maxLat = 85.05112878;
    double clampedLat = std::clamp(lat, -maxLat, maxLat);
    double latRad = clampedLat * M_PI / 180.0;
    double mercY = std::log(std::tan(M_PI / 4.0 + latRad / 2.0));
    double maxMercY =
        std::log(std::tan(M_PI / 4.0 + (maxLat * M_PI / 180.0) / 2.0));
    double ny = 0.5 - 0.5 * (mercY / maxMercY);
    double nx = (lon + 180.0) / 360.0;
    float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
    float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
    return {px, py};
  }
  double nx = (lon + 180.0) / 360.0;
  double ny = (90.0 - lat) / 180.0;
  float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
  float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
  return {px, py};
}

bool MapWidget::screenToLatLon(int sx, int sy, double &lat, double &lon) const {
  if (sx < mapRect_.x || sx > mapRect_.x + mapRect_.w || sy < mapRect_.y ||
      sy > mapRect_.y + mapRect_.h)
    return false;

  if (config_.projection == "robinson") {
    double rnx =
        (static_cast<double>(sx - mapRect_.x) / mapRect_.w) * 2.0 - 1.0;
    double rny =
        1.0 - (static_cast<double>(sy - mapRect_.y) / mapRect_.h) * 2.0;
    inverseRobinson(rnx, rny, lat, lon);
    return true;
  }
  if (config_.projection == "mercator") {
    double nx = static_cast<double>(sx - mapRect_.x) / mapRect_.w;
    double ny = static_cast<double>(sy - mapRect_.y) / mapRect_.h;
    lon = nx * 360.0 - 180.0;
    constexpr double maxLat = 85.05112878;
    double maxMercY =
        std::log(std::tan(M_PI / 4.0 + (maxLat * M_PI / 180.0) / 2.0));
    double mercY = (0.5 - ny) * 2.0 * maxMercY;
    lat = (2.0 * std::atan(std::exp(mercY)) - M_PI / 2.0) * 180.0 / M_PI;
    return true;
  }

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
  // Always update menu if visible
  if (mapViewMenu_->isVisible()) {
    mapViewMenu_->update();
  }

  uint32_t nowMs = SDL_GetTicks();

  // General 1-second updates
  if (nowMs - lastPosUpdateMs_ > 1000) {
    auto now = std::chrono::system_clock::now();
    auto sun = Astronomy::sunPosition(now);
    sunLat_ = sun.lat;
    sunLon_ = sun.lon;
    lastPosUpdateMs_ = nowMs;
  }

  // Satellite ground track update (every 5 seconds)
  if (predictor_ && predictor_->isReady() && config_.showSatTrack) {
    if (nowMs - lastSatTrackUpdateMs_ > 5000) {
      lastSatTrackUpdateMs_ = nowMs;

      // Offload the expensive calculation to a worker thread.
      WorkerService::getInstance().submitTask([this] {
        auto *track_ptr = new std::vector<GroundTrackPoint>();
        *track_ptr =
            predictor_->groundTrack(std::chrono::system_clock::to_time_t(
                                        std::chrono::system_clock::now()),
                                    90, 30);

        SDL_Event event;
        SDL_zero(event);
        event.type =
            HamClock::AE_BASE_EVENT + HamClock::AE_SATELLITE_TRACK_READY;
        event.user.data1 = track_ptr;
        SDL_PushEvent(&event);
      });
    }
  } else if (!cachedSatTrack_.empty()) {
    cachedSatTrack_.clear();
    satTrackDirty_ = true;
  }

  // Great Circle update (on change)
  if (state_->dxActive) {
    if (state_->deLocation.lat != lastDE_.lat ||
        state_->deLocation.lon != lastDE_.lon ||
        state_->dxLocation.lat != lastDX_.lat ||
        state_->dxLocation.lon != lastDX_.lon) {
      int segments = useCompatibilityRenderPath_ ? 100 : 250;
      cachedGreatCircle_ = Astronomy::calculateGreatCirclePath(
          state_->deLocation, state_->dxLocation, segments);
      lastDE_ = state_->deLocation;
      lastDX_ = state_->dxLocation;
      greatCircleDirty_ = true;
    }
  } else if (!cachedGreatCircle_.empty()) {
    cachedGreatCircle_.clear();
    greatCircleDirty_ = true;
  }

  // Monthly map texture update
  auto now_for_month = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now_for_month);
  std::tm *tm = std::localtime(&t);
  int month = tm->tm_mon + 1; // 1-12

  if (month != currentMonth_) {
    currentMonth_ = month;

    char url[256];
    const std::string &style = config_.mapStyle;
    if (style == "topo_bathy") {
      std::snprintf(
          url, sizeof(url),
          "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/"
          "bmng/bmng-topography-bathymetry/%s/"
          "world.topo.bathy.2004%02d.3x5400x2700.jpg",
          kMonthNames[month - 1], month);
    } else if (style == "topo") {
      std::snprintf(
          url, sizeof(url),
          "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/"
          "bmng/bmng-topography/%s/"
          "world.topo.2004%02d.3x5400x2700.jpg",
          kMonthNames[month - 1], month);
    } else {
      if (style != "nasa") {
        LOG_W("MapWidget", "Unknown map style '{}', falling back to 'nasa'",
              style);
      }
      std::snprintf(
          url, sizeof(url),
          "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/"
          "bmng/bmng-base/%s/world.2004%02d.3x5400x2700.jpg",
          kMonthNames[month - 1], month);
    }

    LOG_I("MapWidget", "Starting async fetch for {}", url);
    netMgr_.fetchAsync(
        url,
        [this, url_str = std::string(url)](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for {}", data.size(),
                  url_str);
            std::lock_guard<std::mutex> lock(mapDataMutex_);
            pendingMapData_ = std::move(data);
          } else {
            LOG_E("MapWidget", "Fetch failed or empty for {}", url_str);
          }
        },
        86400 * 30); // Cache for a month

    const char *nightUrl = "https://eoimages.gsfc.nasa.gov/images/imagerecords/"
                           "79000/79765/dnb_land_ocean_ice.2012.3600x1800.jpg";
    LOG_I("MapWidget", "Starting async fetch for Night Lights");
    netMgr_.fetchAsync(
        nightUrl,
        [this, nightUrlStr = std::string(nightUrl)](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for Night Lights",
                  data.size());
            std::lock_guard<std::mutex> lock(mapDataMutex_);
            pendingNightMapData_ = std::move(data);
          } else {
            LOG_E("MapWidget", "Night Lights fetch failed for {}", nightUrlStr);
          }
        },
        86400 * 365); // Cache for a year
  }

  if (config_.propOverlay != PropOverlayType::None) {
    bool needUpdate = false;
    if (iono_ && iono_->hasData()) {
      uint32_t lastUp = iono_->getLastUpdateMs();
      if (lastUp != lastMufUpdateMs_) {
        needUpdate = true;
      }
    } else if (mufrt_ && mufrt_->hasData()) {
      uint32_t lastUp = mufrt_->getLastUpdateMs();
      if (lastUp != lastMufUpdateMs_) {
        std::lock_guard<std::mutex> lock(mapDataMutex_);
        pendingMufData_ = mufrt_->getData();
        lastMufUpdateMs_ = lastUp;
      }
    }

    if (needUpdate && iono_ && solar_) {
      uint32_t now = SDL_GetTicks();
      if (now - lastMufUpdateMs_ > 5000) { // Throttle generation
        lastMufUpdateMs_ = now;
      }
    }
  }
}
bool MapWidget::onMouseUp(int mx, int my, Uint16 mod) {
  // Pass through to menu if visible
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onMouseUp(mx, my, mod);
  }

  // Check RSS toggle button (lower-left corner)
  if (mx >= rssRect_.x && mx < rssRect_.x + rssRect_.w && my >= rssRect_.y &&
      my < rssRect_.y + rssRect_.h) {
    config_.rssEnabled = !config_.rssEnabled;
    if (onConfigChanged_)
      onConfigChanged_();
    return true;
  }

  // Check map view menu button
  if (mx >= projRect_.x && mx < projRect_.x + projRect_.w &&
      my >= projRect_.y && my < projRect_.y + projRect_.h) {
    mapViewMenu_->show(config_, [this]() {
      LOG_D("MapWidget",
            "Map view settings changed: projection={}, style={}, "
            "grid={} ({})",
            config_.projection, config_.mapStyle,
            config_.showGrid ? "ON" : "OFF", config_.gridType);
      if (onConfigChanged_)
        onConfigChanged_();
      // Force map and geometry reload
      mapLoaded_ = false;
      currentMonth_ = 0; // Trigger month update
      greatCircleDirty_ = true;
      satTrackDirty_ = true;
      gridDirty_ = true;
      mapVerts_.clear();
    });
    return true;
  }

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

bool MapWidget::onMouseWheel(int scrollY) {
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onMouseWheel(scrollY);
  }
  return false;
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
    auto data = dxcStore_->snapshot();
    if (!data->spots.empty()) {
      for (const auto &spot : data->spots) {
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
  if (cachedGreatCircle_.empty())
    return;

  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  if (!lineTex)
    return;

  if (greatCircleDirty_) {
    greatCircleVerts_.clear();
    greatCircleIndices_.clear();

    const auto &path = cachedGreatCircle_;
    float thickness = 1.2f;
    float r = thickness / 2.0f;
    SDL_Color color = {255, 255, 0, 255}; // Yellow

    std::vector<SDL_FPoint> segment;
    auto add_segment_geom = [&](const std::vector<SDL_FPoint> &seg) {
      for (size_t i = 1; i < seg.size(); i++) {
        SDL_FPoint p1 = seg[i - 1];
        SDL_FPoint p2 = seg[i];
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.1f)
          continue;

        float nx = -dy / len * r;
        float ny = dx / len * r;

        int base = static_cast<int>(greatCircleVerts_.size());
        greatCircleVerts_.push_back({{p1.x + nx, p1.y + ny}, color, {0, 0}});
        greatCircleVerts_.push_back({{p1.x - nx, p1.y - ny}, color, {0, 1}});
        greatCircleVerts_.push_back({{p2.x + nx, p2.y + ny}, color, {1, 0}});
        greatCircleVerts_.push_back({{p2.x - nx, p2.y - ny}, color, {1, 1}});

        greatCircleIndices_.push_back(base + 0);
        greatCircleIndices_.push_back(base + 1);
        greatCircleIndices_.push_back(base + 2);
        greatCircleIndices_.push_back(base + 1);
        greatCircleIndices_.push_back(base + 2);
        greatCircleIndices_.push_back(base + 3);
      }
    };

    for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0) {
        double lon0 = path[i - 1].lon;
        double lon1 = path[i].lon;
        if (std::fabs(lon0 - lon1) > 180.0) {
          double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
          double borderLon = (lon1 < 0) ? 180.0 : -180.0;
          double f = (borderLon - lon0) / (lon1_adj - lon0);
          double borderLat =
              path[i - 1].lat + f * (path[i].lat - path[i - 1].lat);

          segment.push_back(latLonToScreen(borderLat, borderLon));
          add_segment_geom(segment);
          segment.clear();
          segment.push_back(latLonToScreen(borderLat, -borderLon));
        }
      }
      segment.push_back(latLonToScreen(path[i].lat, path[i].lon));
    }
    if (segment.size() >= 2) {
      add_segment_geom(segment);
    }
    greatCircleDirty_ = false;
  }

  if (!greatCircleVerts_.empty()) {
    SDL_RenderGeometry(renderer, lineTex, greatCircleVerts_.data(),
                       (int)greatCircleVerts_.size(),
                       greatCircleIndices_.data(),
                       (int)greatCircleIndices_.size());
  }
}

void MapWidget::renderNightOverlay(SDL_Renderer *renderer) {
  const float sLatRad = sunLat_ * M_PI / 180.0;
  const float sLonRad = sunLon_ * M_PI / 180.0;
  const float sinSLat = std::sin(sLatRad);
  const float cosSLat = std::cos(sLatRad);

  // Low-memory mode: reduce mesh density on KMSDRM to minimize GPU allocations
  const int gridW = useCompatibilityRenderPath_ ? 48 : 96;
  const int gridH = useCompatibilityRenderPath_ ? 24 : 48;

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
  // High-Fidelity Path (SDL. 2.0.18+)
  // -------------------------------------------------------------------------

  // Force blend mode for geometry shading
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  // Reuse buffers and only recompute if the sun has moved or size changed
  bool needsUpdate =
      (std::abs(lastUpdateSunLat_ - sunLat_) > 0.001 ||
       std::abs(lastUpdateSunLon_ - sunLon_) > 0.001 || shadowVerts_.empty());

  if (shadowVerts_.size() != (size_t)((gridW + 1) * (gridH + 1))) {
    shadowVerts_.resize((gridW + 1) * (gridH + 1));
    lightVerts_.resize((gridW + 1) * (gridH + 1));
    needsUpdate = true;
  }

  if (needsUpdate) {
    for (int j = 0; j <= gridH; ++j) {
      float sy = mapRect_.y + (float)j * mapRect_.h / gridH;
      for (int i = 0; i <= gridW; ++i) {
        float sx = mapRect_.x + (float)i * mapRect_.w / gridW;
        int idx = j * (gridW + 1) + i;

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

          // Projection-aware texture coordinates for night lights
          float u = static_cast<float>((lon + 180.0) / 360.0);
          float v = static_cast<float>((90.0 - lat) / 180.0);
          shadowVerts_[idx] = {
              {sx, sy}, {255, 255, 255, (Uint8)(nf * 255)}, {0, 0}};
          lightVerts_[idx] = {
              {sx, sy}, {255, 255, 255, (Uint8)(nf * 255)}, {u, v}};
        } else {
          shadowVerts_[idx] = {{sx, sy}, {0, 0, 0, 0}, {0, 0}};
          lightVerts_[idx] = {{sx, sy}, {0, 0, 0, 0}, {0, 0}};
        }
      }
    }
    lastUpdateSunLat_ = sunLat_;
    lastUpdateSunLon_ = sunLon_;
  }

  if (nightIndices_.size() != (size_t)(gridW * gridH * 6)) {
    nightIndices_.clear();
    nightIndices_.reserve(gridW * gridH * 6);
    for (int j = 0; j < gridH; ++j) {
      for (int i = 0; i < gridW; ++i) {
        int p0 = j * (gridW + 1) + i;
        int p1 = p0 + 1;
        int p2 = (j + 1) * (gridW + 1) + i;
        int p3 = p2 + 1;
        nightIndices_.push_back(p0);
        nightIndices_.push_back(p1);
        nightIndices_.push_back(p2);
        nightIndices_.push_back(p2);
        nightIndices_.push_back(p1);
        nightIndices_.push_back(p3);
      }
    }
  }

  // Draw shaded overlay using a BLACK texture and WHITE vertex colors
  SDL_Texture *blackTex = texMgr_.get("black");
  if (blackTex) {
    SDL_RenderGeometry(renderer, blackTex, shadowVerts_.data(),
                       (int)shadowVerts_.size(), nightIndices_.data(),
                       (int)nightIndices_.size());
  } else {
    LOG_W("MapWidget", "Black texture not available for night overlay");
  }

  if (config_.mapNightLights) {
    SDL_Texture *nightTex = texMgr_.get(NIGHT_MAP_KEY);
    if (nightTex) {
      SDL_SetTextureColorMod(nightTex, 255, 255, 255);
      SDL_SetTextureBlendMode(nightTex, SDL_BLENDMODE_BLEND);
      SDL_RenderGeometry(renderer, nightTex, lightVerts_.data(),
                         (int)lightVerts_.size(), nightIndices_.data(),
                         (int)nightIndices_.size());
    }
  }
#else
  // -------------------------------------------------------------------------
  // Compatibility Path (SDL < 2.0.18)
  // -------------------------------------------------------------------------
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  // 1. Draw shading
  for (int j = 0; j < gridH; ++j) {
    int y1 = mapRect_.y + j * mapRect_.h / gridH;
    int y2 = mapRect_.y + (j + 1) * mapRect_.h / gridH;
    for (int i = 0; i < gridW; ++i) {
      int x1 = mapRect_.x + i * mapRect_.w / gridW;
      int x2 = mapRect_.x + (i + 1) * mapRect_.w / gridW;
      double lat, lon;
      if (screenToLatLon(x1 + (x2 - x1) / 2, y1 + (y2 - y1) / 2, lat, lon)) {

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
          SDL_Rect r = {x1, y1, x2 - x1, y2 - y1};
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
        int y1 = mapRect_.y + j * mapRect_.h / gridH;
        int y2 = mapRect_.y + (j + 1) * mapRect_.h / gridH;
        for (int i = 0; i < gridW; ++i) {
          int x1 = mapRect_.x + i * mapRect_.w / gridW;
          int x2 = mapRect_.x + (i + 1) * mapRect_.w / gridW;

          // Darkness factor at center of cell
          float fd = 1.0f;
          double lat, lon;
          if (screenToLatLon(x1 + (x2 - x1) / 2, y1 + (y2 - y1) / 2, lat,
                             lon)) {

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
            // Re-calculate projection-aware u,v for legacy path
            float u = static_cast<float>((lon + 180.0) / 360.0);
            float v = static_cast<float>((90.0 - lat) / 180.0);

            SDL_SetTextureAlphaMod(nightTex, (Uint8)(darkness * 255));
            SDL_Rect src = {(int)(u * tW), (int)(v * tH), (int)(srcStepX),
                            (int)(srcStepY)};

            SDL_Rect dst = {x1, y1, x2 - x1, y2 - y1};

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
      SDL_Texture *mapTex =
          texMgr_.loadFromMemory(renderer, MAP_KEY, pendingMapData_);
      if (mapTex) {
        SDL_SetTextureBlendMode(mapTex, SDL_BLENDMODE_NONE);
      } else {
        LOG_E("MapWidget", "Failed to create map texture from {} bytes: {}",
              pendingMapData_.size(), SDL_GetError());
      }
      // Clear pending data even on failure to prevent retry loops
      pendingMapData_.clear();
    }
    if (!pendingNightMapData_.empty()) {
      SDL_Texture *nightTex =
          texMgr_.loadFromMemory(renderer, NIGHT_MAP_KEY, pendingNightMapData_);
      if (!nightTex) {
        LOG_E("MapWidget",
              "Failed to create night map texture from {} bytes: {}",
              pendingNightMapData_.size(), SDL_GetError());
      }
      // Clear pending data even on failure to prevent retry loops
      pendingNightMapData_.clear();
    }
    if (!pendingMufData_.empty()) {
      SDL_Texture *tex =
          texMgr_.loadFromMemory(renderer, "muf_rt_overlay", pendingMufData_);
      if (!tex) {
        LOG_E("MapWidget", "Failed to create MUF texture: {}", SDL_GetError());
      }
      pendingMufData_.clear();
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
  if (mapTex) {
    if (config_.projection != "equirectangular") {
      // Draw using mesh to support Robinson or Mercator warping
      // Low-memory mode: reduce mesh density on KMSDRM
      const int gridW = useCompatibilityRenderPath_ ? 48 : 96;
      const int gridH = useCompatibilityRenderPath_ ? 24 : 48;
      bool needsMeshUpdate = mapVerts_.empty() ||
                             (mapVerts_.size() != (gridW + 1) * (gridH + 1)) ||
                             (lastProjection_ != config_.projection);

      if (needsMeshUpdate) {
        lastProjection_ = config_.projection;
        mapVerts_.resize((gridW + 1) * (gridH + 1));
        for (int j = 0; j <= gridH; ++j) {
          float v = (float)j / gridH;
          double lat = 90.0 - v * 180.0;
          for (int i = 0; i <= gridW; ++i) {
            float u = (float)i / gridW;
            double lon = u * 360.0 - 180.0;
            SDL_FPoint screen = latLonToScreen(lat, lon);
            mapVerts_[j * (gridW + 1) + i] = {
                screen, {255, 255, 255, 255}, {u, v}};
          }
        }

        // Also ensure indices are ready
        if (nightIndices_.size() != (size_t)(gridW * gridH * 6)) {
          nightIndices_.clear();
          nightIndices_.reserve(gridW * gridH * 6);
          for (int j = 0; j < gridH; ++j) {
            for (int i = 0; i < gridW; ++i) {
              int p0 = j * (gridW + 1) + i;
              int p1 = p0 + 1;
              int p2 = (j + 1) * (gridW + 1) + i;
              int p3 = p2 + 1;
              nightIndices_.push_back(p0);
              nightIndices_.push_back(p1);
              nightIndices_.push_back(p2);
              nightIndices_.push_back(p2);
              nightIndices_.push_back(p1);
              nightIndices_.push_back(p3);
            }
          }
        }
      }

      SDL_RenderGeometry(renderer, mapTex, mapVerts_.data(),
                         (int)mapVerts_.size(), nightIndices_.data(),
                         (int)nightIndices_.size());
    } else {
      SDL_RenderCopy(renderer, mapTex, nullptr, &mapRect_);
    }
  }

  renderMufRtOverlay(renderer);
  renderNightOverlay(renderer);
  renderGridOverlay(renderer);
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
  renderADIFPins(renderer);
  renderONTASpots(renderer);

  renderMarker(renderer, sunLat_, sunLon_, 255, 255, 0, MarkerShape::Circle,
               true);

  renderProjectionSelect(renderer);
  renderRssButton(renderer);
  renderOverlayInfo(renderer);
  renderTooltip(renderer);

  // Note: MapViewMenu is rendered via renderModal() in the centralized modal
  // pass, not here. This prevents clipping to the map pane bounds.

  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);
}

void MapWidget::renderSatellite(SDL_Renderer *renderer) {
  if (!predictor_ || !predictor_->isReady())
    return;
  SubSatPoint ssp = predictor_->subSatPoint();
  if (config_.showSatTrack) {
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

  // Low-memory mode: reduce footprint segments on KMSDRM
  const int kSegments = useCompatibilityRenderPath_ ? 36 : 72;
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

    if (i > 0) {
      double prevLon =
          lon + angRadDeg * std::sin(2.0 * M_PI * (i - 1) / kSegments) / cosLat;
      while (prevLon > 180.0)
        prevLon -= 360.0;
      while (prevLon < -180.0)
        prevLon += 360.0;

      if (std::abs(pLon - prevLon) > 180.0) {
        // Crossing Date Line
        double lon1 = prevLon;
        double lon2 = pLon;
        double lon2_adj = (lon2 < 0) ? lon2 + 360.0 : lon2 - 360.0;
        double borderLon = (lon2 < 0) ? 180.0 : -180.0;
        double f = (borderLon - lon1) / (lon2_adj - lon1);
        double prevLat =
            lat + angRadDeg * std::cos(2.0 * M_PI * (i - 1) / kSegments);
        double borderLat = prevLat + f * (pLat - prevLat);

        segment.push_back(latLonToScreen(borderLat, borderLon));
        if (segment.size() >= 2) {
          RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                            static_cast<int>(segment.size()),
                                            2.0f, {255, 255, 0, 120});
        }
        segment.clear();
        segment.push_back(latLonToScreen(borderLat, -borderLon));
      }
    }

    segment.push_back(latLonToScreen(pLat, pLon));
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                      static_cast<int>(segment.size()), 2.0f,
                                      {255, 255, 0, 120});
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSatGroundTrack(SDL_Renderer *renderer) {
  if (cachedSatTrack_.size() < 2)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  if (!lineTex)
    return;

  if (satTrackDirty_) {
    satTrackVerts_.clear();
    satTrackIndices_.clear();

    float thickness = 1.5f;
    float r = thickness / 2.0f;
    SDL_Color color = {255, 200, 0, 150};

    std::vector<SDL_FPoint> segment;
    auto add_segment_geom = [&](const std::vector<SDL_FPoint> &seg) {
      for (size_t i = 1; i < seg.size(); i++) {
        SDL_FPoint p1 = seg[i - 1];
        SDL_FPoint p2 = seg[i];
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.1f)
          continue;

        float nx = -dy / len * r;
        float ny = dx / len * r;

        int base = static_cast<int>(satTrackVerts_.size());
        satTrackVerts_.push_back({{p1.x + nx, p1.y + ny}, color, {0, 0}});
        satTrackVerts_.push_back({{p1.x - nx, p1.y - ny}, color, {0, 1}});
        satTrackVerts_.push_back({{p2.x + nx, p2.y + ny}, color, {1, 0}});
        satTrackVerts_.push_back({{p2.x - nx, p2.y - ny}, color, {1, 1}});

        satTrackIndices_.push_back(base + 0);
        satTrackIndices_.push_back(base + 1);
        satTrackIndices_.push_back(base + 2);
        satTrackIndices_.push_back(base + 1);
        satTrackIndices_.push_back(base + 2);
        satTrackIndices_.push_back(base + 3);
      }
    };

    for (size_t i = 0; i < cachedSatTrack_.size(); ++i) {
      if (i > 0) {
        double lon0 = cachedSatTrack_[i - 1].lon;
        double lon1 = cachedSatTrack_[i].lon;
        if (std::fabs(lon0 - lon1) > 180.0) {
          double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
          double borderLon = (lon1 < 0) ? 180.0 : -180.0;
          double f = (borderLon - lon0) / (lon1_adj - lon0);
          double borderLat =
              cachedSatTrack_[i - 1].lat +
              f * (cachedSatTrack_[i].lat - cachedSatTrack_[i - 1].lat);

          segment.push_back(latLonToScreen(borderLat, borderLon));
          add_segment_geom(segment);
          segment.clear();
          segment.push_back(latLonToScreen(borderLat, -borderLon));
        }
      }
      segment.push_back(
          latLonToScreen(cachedSatTrack_[i].lat, cachedSatTrack_[i].lon));
    }
    if (segment.size() >= 2) {
      add_segment_geom(segment);
    }
    satTrackDirty_ = false;
  }

  if (!satTrackVerts_.empty()) {
    SDL_RenderGeometry(renderer, lineTex, satTrackVerts_.data(),
                       (int)satTrackVerts_.size(), satTrackIndices_.data(),
                       (int)satTrackIndices_.size());
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSpotOverlay(SDL_Renderer *renderer) {
  if (!spotStore_)
    return;
  auto data = spotStore_->snapshot();
  if (!data->valid || data->spots.empty())
    return;

  bool anySelected = false;
  for (int i = 0; i < kNumBands; ++i) {
    if (data->selectedBands[i]) {
      anySelected = true;
      break;
    }
  }
  if (!anySelected)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  LatLon de = state_->deLocation;
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  SDL_Texture *markerTex = texMgr_.get("marker_square");
  if (!lineTex || !markerTex)
    return;

  spotVerts_.clear();
  spotIndices_.clear();
  markerVerts_.clear();
  markerIndices_.clear();

  int renderedCount = 0;
  const int MAX_MAP_SPOTS = useCompatibilityRenderPath_ ? 100 : 200;

  for (const auto &spot : data->spots) {
    if (renderedCount >= MAX_MAP_SPOTS)
      break;

    int bandIdx = freqToBandIndex(spot.freqKhz);
    if (bandIdx < 0 || !data->selectedBands[bandIdx])
      continue;

    double lat, lon;
    if (!Astronomy::gridToLatLon(spot.receiverGrid, lat, lon))
      continue;

    renderedCount++;
    const auto &bc = kBands[bandIdx].color;
    SDL_Color color = {bc.r, bc.g, bc.b, 180};
    SDL_Color mColor = {bc.r, bc.g, bc.b, 255};

    int segments = useCompatibilityRenderPath_ ? 20 : 100;
    auto path = Astronomy::calculateGreatCirclePath(de, {lat, lon}, segments);

    // Batch Lines
    float thickness = 1.3f;

    float r = thickness / 2.0f;

    auto addLine = [&](SDL_FPoint p1, SDL_FPoint p2) {
      float dx = p2.x - p1.x;
      float dy = p2.y - p1.y;
      float len = std::sqrt(dx * dx + dy * dy);
      if (len < 0.1f)
        return;

      float nx = -dy / len * r;
      float ny = dx / len * r;

      int base = static_cast<int>(spotVerts_.size());
      spotVerts_.push_back({{p1.x + nx, p1.y + ny}, color, {0, 0}});
      spotVerts_.push_back({{p1.x - nx, p1.y - ny}, color, {0, 1}});
      spotVerts_.push_back({{p2.x + nx, p2.y + ny}, color, {1, 0}});
      spotVerts_.push_back({{p2.x - nx, p2.y - ny}, color, {1, 1}});

      spotIndices_.push_back(base + 0);
      spotIndices_.push_back(base + 1);
      spotIndices_.push_back(base + 2);
      spotIndices_.push_back(base + 1);
      spotIndices_.push_back(base + 2);
      spotIndices_.push_back(base + 3);
    };

    for (size_t i = 1; i < path.size(); ++i) {
      double lon0 = path[i - 1].lon;
      double lon1 = path[i].lon;

      if (std::fabs(lon0 - lon1) > 180.0) {
        double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
        double borderLon = (lon1 < 0) ? 180.0 : -180.0;
        double f = (borderLon - lon0) / (lon1_adj - lon0);
        double borderLat =
            path[i - 1].lat + f * (path[i].lat - path[i - 1].lat);

        SDL_FPoint p0 = latLonToScreen(path[i - 1].lat, path[i - 1].lon);
        SDL_FPoint pE1 = latLonToScreen(borderLat, borderLon);
        addLine(p0, pE1);

        SDL_FPoint pE2 = latLonToScreen(borderLat, -borderLon);
        SDL_FPoint p1 = latLonToScreen(path[i].lat, path[i].lon);
        addLine(pE2, p1);
      } else {
        SDL_FPoint p0 = latLonToScreen(path[i - 1].lat, path[i - 1].lon);
        SDL_FPoint p1 = latLonToScreen(path[i].lat, path[i].lon);
        addLine(p0, p1);
      }
    }

    // Batch Marker (as a small quad)
    SDL_FPoint mPt = latLonToScreen(lat, lon);
    float mSize = 3.0f;
    int mBase = static_cast<int>(markerVerts_.size());
    markerVerts_.push_back({{mPt.x - mSize, mPt.y - mSize}, mColor, {0, 0}});
    markerVerts_.push_back({{mPt.x + mSize, mPt.y - mSize}, mColor, {1, 0}});
    markerVerts_.push_back({{mPt.x - mSize, mPt.y + mSize}, mColor, {0, 1}});
    markerVerts_.push_back({{mPt.x + mSize, mPt.y + mSize}, mColor, {1, 1}});

    markerIndices_.push_back(mBase + 0);
    markerIndices_.push_back(mBase + 1);
    markerIndices_.push_back(mBase + 2);
    markerIndices_.push_back(mBase + 1);
    markerIndices_.push_back(mBase + 2);
    markerIndices_.push_back(mBase + 3);
  }

  if (!spotVerts_.empty()) {
    SDL_RenderGeometry(renderer, lineTex, spotVerts_.data(),
                       (int)spotVerts_.size(), spotIndices_.data(),
                       (int)spotIndices_.size());
  }
  if (!markerVerts_.empty()) {
    SDL_RenderGeometry(renderer, markerTex, markerVerts_.data(),
                       (int)markerVerts_.size(), markerIndices_.data(),
                       (int)markerIndices_.size());
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderDXClusterSpots(SDL_Renderer *renderer) {
  if (!dxcStore_)
    return;
  auto data = dxcStore_->snapshot();
  if (data->spots.empty())
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  // Filter spots to render
  std::vector<DXClusterSpot> spotsToRender;
  if (data->hasSelection) {
    spotsToRender.push_back(data->selectedSpot);
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
          {spot.rxLat, spot.rxLon}, {spot.txLat, spot.txLon}, 100);

      std::vector<SDL_FPoint> segment;
      SDL_Color lineColor = {color.r, color.g, color.b, 100};

      for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) {
          double lon0 = path[i - 1].lon;
          double lon1 = path[i].lon;
          if (std::fabs(lon0 - lon1) > 180.0) {
            double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
            double borderLon = (lon1 < 0) ? 180.0 : -180.0;
            double f = (borderLon - lon0) / (lon1_adj - lon0);
            double borderLat =
                path[i - 1].lat + f * (path[i].lat - path[i - 1].lat);

            segment.push_back(latLonToScreen(borderLat, borderLon));
            if (segment.size() >= 2) {
              RenderUtils::drawPolylineTextured(
                  renderer, lineTex, segment.data(),
                  static_cast<int>(segment.size()), 1.0f, lineColor);
            }
            segment.clear();
            segment.push_back(latLonToScreen(borderLat, -borderLon));
          }
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

void MapWidget::renderADIFPins(SDL_Renderer *renderer) {
  if (!adifStore_)
    return;
  auto stats = adifStore_->get();
  if (!stats.valid || stats.recentQSOs.empty())
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);

  for (const auto &qso : stats.recentQSOs) {
    if (qso.lat == 0.0 && qso.lon == 0.0)
      continue;

    // Check filter
    if (!stats.activeBandFilter.empty() && stats.activeBandFilter != "All") {
      if (qso.band != stats.activeBandFilter)
        continue;
    }
    if (!stats.activeModeFilter.empty() && stats.activeModeFilter != "All") {
      if (qso.mode != stats.activeModeFilter)
        continue;
    }

    // Determine color based on band
    SDL_Color color = {255, 255, 255, 255}; // Default white
    for (int i = 0; i < kNumBands; ++i) {
      if (qso.band == kBands[i].name) {
        color = kBands[i].color;
        break;
      }
    }

    renderMarker(renderer, qso.lat, qso.lon, color.r, color.g, color.b,
                 MarkerShape::Circle, true);
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderONTASpots(SDL_Renderer *renderer) {
  if (!activityStore_)
    return;

  ActivityData data = activityStore_->get();
  if (!data.hasSelection)
    return;

  const auto &spot = data.selectedSpot;
  if (spot.lat == 0.0 && spot.lon == 0.0)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  // Lime Green for POTA, Cyan for SOTA
  SDL_Color color = (spot.program == "POTA") ? SDL_Color{50, 255, 50, 255}
                                             : SDL_Color{0, 200, 255, 255};

  LatLon de = state_->deLocation;
  auto path = Astronomy::calculateGreatCirclePath(de, {spot.lat, spot.lon}, 100);

  std::vector<SDL_FPoint> segment;
  SDL_Color lineColor = {color.r, color.g, color.b, 100};

  for (size_t i = 0; i < path.size(); ++i) {
    if (i > 0) {
      double lon0 = path[i - 1].lon;
      double lon1 = path[i].lon;
      if (std::fabs(lon0 - lon1) > 180.0) {
        double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
        double borderLon = (lon1 < 0) ? 180.0 : -180.0;
        double f = (borderLon - lon0) / (lon1_adj - lon0);
        double borderLat = path[i - 1].lat + f * (path[i].lat - path[i - 1].lat);

        segment.push_back(latLonToScreen(borderLat, borderLon));
        if (segment.size() >= 2) {
          RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                            static_cast<int>(segment.size()),
                                            1.0f, lineColor);
        }
        segment.clear();
        segment.push_back(latLonToScreen(borderLat, -borderLon));
      }
    }
    segment.push_back(latLonToScreen(path[i].lat, path[i].lon));
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                      static_cast<int>(segment.size()), 1.0f,
                                      lineColor);
  }

  // Use Square markers for ONTA to differentiate from DX Cluster (Circle)
  renderMarker(renderer, spot.lat, spot.lon, color.r, color.g, color.b,
               MarkerShape::Square, true);

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderMufRtOverlay(SDL_Renderer *renderer) {
  if (config_.propOverlay == PropOverlayType::None)
    return;

  // Prefer Native Engine
  if (iono_ && solar_) {
    static uint32_t lastGen = 0;
    static SDL_Texture *nativeTex = nullptr;
    static PropOverlayType lastType = PropOverlayType::None;
    static std::string lastBand = "";
    static std::string lastMode = "";
    static int lastPower = -1;

    uint32_t now = SDL_GetTicks();
    bool typeChanged = (lastType != config_.propOverlay);
    bool bandChanged = (lastBand != config_.propBand);
    bool modeChanged = (lastMode != config_.propMode);
    bool powerChanged = (lastPower != config_.propPower);

    // Update if texture missing, time elapsed, or params changed
    if (!nativeTex || (now - lastGen > 300000) || typeChanged || bandChanged ||
        modeChanged || powerChanged) {
      PropPathParams params;
      params.txLat = state_->deLocation.lat;
      params.txLon = state_->deLocation.lon;
      params.mode = config_.propMode;
      params.watts = (double)config_.propPower;

      // Determine frequency from band
      std::string band = config_.propBand;
      if (band == "80m")
        params.mhz = 3.5;
      else if (band == "60m")
        params.mhz = 5.3;
      else if (band == "40m")
        params.mhz = 7.0;
      else if (band == "30m")
        params.mhz = 10.1;
      else if (band == "20m")
        params.mhz = 14.1;
      else if (band == "15m")
        params.mhz = 21.1;
      else if (band == "10m")
        params.mhz = 28.2;
      else
        params.mhz = 14.1; // Default

      params.watts = 100;
      params.mode = "SSB";

      SolarData sw = solar_->get();

      // Output Type: 0=MUF, 1=Rel
      int outType = (config_.propOverlay == PropOverlayType::Voacap) ? 1 : 0;

      // Generate 660x330 grid
      std::vector<float> grid =
          PropEngine::generateGrid(params, sw, iono_, outType);

      // Convert to pixels (Heatmap)
      int w = PropEngine::MAP_W;
      int h = PropEngine::MAP_H;
      std::vector<uint32_t> pixels(w * h);

      float maxVal = (outType == 1) ? 100.0f : 50.0f; // 100% or 50MHz

      for (size_t i = 0; i < grid.size(); ++i) {
        float val = grid[i];
        float t = val / maxVal;
        t = std::max(0.0f, std::min(t, 1.0f));

        uint8_t r = 0, g = 0, b = 0;
        // Jet-like colormap
        if (t < 0.25f) { // Blue -> Cyan
          float f = t / 0.25f;
          b = 255;
          g = (uint8_t)(f * 255.0f);
        } else if (t < 0.5f) { // Cyan -> Green
          float f = (t - 0.25f) / 0.25f;
          g = 255;
          b = (uint8_t)((1.0f - f) * 255.0f);
        } else if (t < 0.75f) { // Green -> Yellow
          float f = (t - 0.5f) / 0.25f;
          g = 255;
          r = (uint8_t)(f * 255.0f);
        } else { // Yellow -> Red
          float f = (t - 0.75f) / 0.25f;
          r = 255;
          g = (uint8_t)((1.0f - f) * 255.0f);
        }

        uint8_t a = (val > 2.0f) ? 255 : 0;
        pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
      }

      if (nativeTex)
        SDL_DestroyTexture(nativeTex);
      nativeTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STATIC, w, h);
      SDL_UpdateTexture(nativeTex, nullptr, pixels.data(),
                        w * sizeof(uint32_t));
      SDL_SetTextureBlendMode(nativeTex, SDL_BLENDMODE_BLEND);

      lastGen = now;
      lastType = config_.propOverlay;
      lastBand = config_.propBand;
      lastMode = config_.propMode;
      lastPower = config_.propPower;
    }

    if (nativeTex) {
      SDL_SetTextureAlphaMod(nativeTex, (Uint8)(config_.mufRtOpacity * 2.55f));

      if (config_.projection == "robinson") {
        SDL_RenderGeometry(renderer, nativeTex, mapVerts_.data(),
                           (int)mapVerts_.size(), nightIndices_.data(),
                           (int)nightIndices_.size());
      } else {
        SDL_RenderCopy(renderer, nativeTex, nullptr, &mapRect_);
      }
    }
    return;
  }
  // Fallback to legacy MufRtProvider (fetched PNG)
  SDL_Texture *tex = texMgr_.get("muf_rt_overlay");
  if (!tex)
    return;

  SDL_SetTextureAlphaMod(tex, (Uint8)(config_.mufRtOpacity * 2.55f));
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

  if (config_.projection == "robinson") {
    SDL_RenderGeometry(renderer, tex, mapVerts_.data(), (int)mapVerts_.size(),
                       nightIndices_.data(), (int)nightIndices_.size());
  } else {
    SDL_RenderCopy(renderer, tex, nullptr, &mapRect_);
  }
}

void MapWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcMapRect();
  if (nightOverlayTexture_) {
    MemoryMonitor::getInstance().destroyTexture(nightOverlayTexture_);
  }
  // Invalidate all cached geometry that depends on screen coordinates
  gridDirty_ = true;
  greatCircleDirty_ = true;
  satTrackDirty_ = true;
  mapVerts_.clear(); // Also force map mesh regen
}

// --- Tooltip Rendering ---

void MapWidget::renderTooltip(SDL_Renderer *renderer) {
  if (!tooltip_.visible || tooltip_.text.empty()) {
    // Clean up cached texture when tooltip hidden
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
    tooltip_.cachedText.clear();
    return;
  }

  // Fade out after 3 seconds of no motion
  uint32_t age = SDL_GetTicks() - tooltip_.timestamp;
  if (age > 3000) {
    tooltip_.visible = false;
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
    tooltip_.cachedText.clear();
    return;
  }

  int tw = 0, th = 0;
  int ptSize = std::max(9, height_ / 40);

  // Only create new texture if text changed
  if (tooltip_.text != tooltip_.cachedText || !tooltip_.cachedTexture) {
    // Clean up old texture
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);

    // Create new texture
    tooltip_.cachedTexture = fontMgr_.renderText(
        renderer, tooltip_.text, {255, 255, 255, 255}, ptSize, &tw, &th);

    if (!tooltip_.cachedTexture) {
      LOG_E("MapWidget", "Failed to create tooltip texture: {}",
            SDL_GetError());
      return;
    }

    tooltip_.cachedText = tooltip_.text;
    tooltip_.cachedW = tw;
    tooltip_.cachedH = th;
  } else {
    // Reuse cached texture
    tw = tooltip_.cachedW;
    th = tooltip_.cachedH;
  }

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

  // Text (using cached texture)
  SDL_Rect dst = {bx + padX, by + padY, tw, th};
  SDL_RenderCopy(renderer, tooltip_.cachedTexture, nullptr, &dst);
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
  j["projection"] = config_.projection;

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
    auto sd = spotStore_->snapshot();
    j["live_spot_count"] = static_cast<int>(sd->spots.size());
  }
  if (dxcStore_) {
    auto dd = dxcStore_->snapshot();
    j["dxc_spot_count"] = static_cast<int>(dd->spots.size());
    j["dxc_connected"] = dd->connected;
  }

  // Tooltip
  if (tooltip_.visible) {
    j["tooltip"] = tooltip_.text;
  }

  return j;
}

void MapWidget::onSatTrackReady(const std::vector<GroundTrackPoint> &track) {
  cachedSatTrack_ = track;
  satTrackDirty_ = true;
}

void MapWidget::renderGridOverlay(SDL_Renderer *renderer) {
  if (!config_.showGrid)
    return;

  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 128);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  if (config_.gridType == "latlon") {
    // Draw latitude lines every 15 degrees
    for (int lat = -75; lat <= 75; lat += 15) {
      std::vector<SDL_FPoint> points;
      for (int lon = -180; lon <= 180; lon += 5) {
        points.push_back(latLonToScreen(lat, lon));
      }
      for (size_t i = 1; i < points.size(); ++i) {
        SDL_RenderDrawLineF(renderer, points[i - 1].x, points[i - 1].y,
                            points[i].x, points[i].y);
      }
    }

    // Draw longitude lines every 30 degrees
    for (int lon = -180; lon < 180; lon += 30) {
      std::vector<SDL_FPoint> points;
      for (int lat = -85; lat <= 85; lat += 5) {
        points.push_back(latLonToScreen(lat, lon));
      }
      for (size_t i = 1; i < points.size(); ++i) {
        SDL_RenderDrawLineF(renderer, points[i - 1].x, points[i - 1].y,
                            points[i].x, points[i].y);
      }
    }
  } else if (config_.gridType == "maidenhead") {
    // Draw Maidenhead grid (18 fields × 18 fields, each 20° lon × 10° lat)
    for (int field_lon = 0; field_lon < 18; ++field_lon) {
      double lon = -180.0 + field_lon * 20.0;
      std::vector<SDL_FPoint> points;
      for (int lat = -85; lat <= 85; lat += 5) {
        points.push_back(latLonToScreen(lat, lon));
      }
      for (size_t i = 1; i < points.size(); ++i) {
        SDL_RenderDrawLineF(renderer, points[i - 1].x, points[i - 1].y,
                            points[i].x, points[i].y);
      }
    }

    for (int field_lat = 0; field_lat < 18; ++field_lat) {
      double lat = -90.0 + field_lat * 10.0;
      if (lat < -85 || lat > 85)
        continue;
      std::vector<SDL_FPoint> points;
      for (int lon = -180; lon <= 180; lon += 5) {
        points.push_back(latLonToScreen(lat, lon));
      }
      for (size_t i = 1; i < points.size(); ++i) {
        SDL_RenderDrawLineF(renderer, points[i - 1].x, points[i - 1].y,
                            points[i].x, points[i].y);
      }
    }
  }
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

void MapWidget::renderProjectionSelect(SDL_Renderer *renderer) {
  // Show "Map View ▼" to indicate it opens a menu
  std::string label = "Map View \xE2\x96\xBC"; // ▼ in UTF-8

  // Position at top-left of map (within mapRect_)
  projRect_ = {mapRect_.x + 4, mapRect_.y + 4, 100, 22};

  // Draw semi-transparent background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
  SDL_RenderFillRect(renderer, &projRect_);

  // Border
  SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
  SDL_RenderDrawRect(renderer, &projRect_);

  // Text
  fontMgr_.drawText(renderer, label, projRect_.x + projRect_.w / 2,
                    projRect_.y + projRect_.h / 2, {200, 200, 200, 255}, 10,
                    true, true);
}

void MapWidget::renderRssButton(SDL_Renderer *renderer) {
  // Draw "RSS" toggle button at top-right of the map area, symmetric with the
  // "Map View ▼" button at top-left. Positioned here so it is never covered
  // by the RSSBanner, which is a separate widget rendered after MapWidget in
  // the main loop and occupies the bottom strip of the screen.
  // Green border/text = enabled; gray = disabled.
  rssRect_ = {mapRect_.x + mapRect_.w - 48, mapRect_.y + 4, 44, 22};

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
  SDL_RenderFillRect(renderer, &rssRect_);

  SDL_Color col = config_.rssEnabled ? SDL_Color{80, 220, 80, 255}
                                     : SDL_Color{90, 90, 90, 255};
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  SDL_RenderDrawRect(renderer, &rssRect_);

  fontMgr_.drawText(renderer, "RSS", rssRect_.x + rssRect_.w / 2,
                    rssRect_.y + rssRect_.h / 2, col, 10, false, true);
}

void MapWidget::renderOverlayInfo(SDL_Renderer *renderer) {
  if (config_.propOverlay == PropOverlayType::None)
    return;

  std::string text;
  if (config_.propOverlay == PropOverlayType::Muf) {
    text = "MUF Overlay";
  } else if (config_.propOverlay == PropOverlayType::Voacap) {
    text = fmt::format("VOACAP ({} / {} / {}W)", config_.propBand,
                       config_.propMode, config_.propPower);
  }

  if (text.empty())
    return;

  int ptSize = 14;
  int textW = fontMgr_.getLogicalWidth(text, ptSize, true);
  int textH = 20; // Approx for 14pt (simplified)
  int padX = 12;
  int padY = 4;
  int boxW = textW + padX * 2;
  int boxH = textH + padY * 2;

  int cx = mapRect_.x + mapRect_.w / 2;
  int cy = mapRect_.y + 20; // Top margin

  SDL_Rect box = {cx - boxW / 2, cy - boxH / 2, boxW, boxH};

  // Box
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180); // Dark semi-transparent
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255); // Border
  SDL_RenderDrawRect(renderer, &box);

  // Text
  fontMgr_.drawText(renderer, text, cx, cy, {255, 255, 255, 255}, ptSize, true,
                    true);
}

// Modal interface implementation
bool MapWidget::isModalActive() const {
  return mapViewMenu_ && mapViewMenu_->isVisible();
}

void MapWidget::renderModal(SDL_Renderer *renderer) {
  if (mapViewMenu_ && mapViewMenu_->isVisible()) {
    mapViewMenu_->render(renderer);
  }
}
