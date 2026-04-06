#ifdef _WIN32
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif
#include "MapWidget.h"
#include "../core/AsteroidPropagator.h"
#include "../core/Astronomy.h"
#include "../core/BeaconData.h"
#include "../core/Constants.h"
#include "../core/LiveSpotData.h"
#include "../core/Logger.h"
#include "../core/PropEngine.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include "../core/WorldBorders.h"
#include "../services/AsteroidProvider.h"
#include "../services/BeaconProvider.h"
#include "../services/GribCloudProvider.h"
#include "../services/IonosondeProvider.h"
#include "../services/WxMbProvider.h"
#include "EmbeddedIcons.h"
#include "FontCatalog.h"
#include "PaneContainer.h"
#include "RenderUtils.h"
#include "../core/ConfigManager.h"
#include <fmt/core.h>

#include <algorithm>

#include <SDL_video.h>
#include <chrono>
#include <ctime>
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

float MapWidget::getRobinsonXCoeff(double lat) {
  double abs_lat = std::abs(lat);
  if (abs_lat > 90.0)
    abs_lat = 90.0;
  int idx = static_cast<int>(abs_lat / 5.0);
  if (idx >= 18)
    idx = 17;
  double remainder = (abs_lat - idx * 5.0) / 5.0;
  return robinson_coeffs[idx].x +
         (robinson_coeffs[idx + 1].x - robinson_coeffs[idx].x) * remainder;
}

static void projectRobinson(double lat, double lon, double &nx, double &ny) {
  double abs_lat = std::abs(lat);
  if (abs_lat > 90.0)
    abs_lat = 90.0;
  int idx = static_cast<int>(abs_lat / 5.0);
  if (idx >= 18)
    idx = 17;
  double remainder = (abs_lat - idx * 5.0) / 5.0;

  double x_coeff = MapWidget::getRobinsonXCoeff(lat);
  double y_coeff =
      robinson_coeffs[idx].y +
      (robinson_coeffs[idx + 1].y - robinson_coeffs[idx].y) * remainder;

  nx = (lon / 180.0) * x_coeff;  // [-1, 1]
  ny = (lat < 0) ? -y_coeff : y_coeff;  // [-1, 1]
}

static bool inverseRobinson(double nx, double ny, double &lat, double &lon) {
  if (std::abs(ny) > 1.0)
    return false;

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
  double x_coeff = MapWidget::getRobinsonXCoeff(lat);
  if (x_coeff < 0.01)
    x_coeff = 0.01;

  bool inside = (std::abs(nx) <= x_coeff);
  lon = (nx / x_coeff) * 180.0;
  if (lon > 180.0)
    lon = 180.0;
  if (lon < -180.0)
    lon = -180.0;
  return inside;
}

// Azimuthal equidistant projection helpers (all angles in degrees)
// nx, ny are normalized to [-1, 1] where 1.0 == π radians from center.
void projectAzimuthal(double lat, double lon, double lat0, double lon0,
                      double &nx, double &ny) {
  const double D = M_PI / 180.0;
  double phi = lat * D, phi0 = lat0 * D;
  double dlon = (lon - lon0) * D;
  double cos_c = std::clamp(
      sin(phi0) * sin(phi) + cos(phi0) * cos(phi) * cos(dlon), -1.0, 1.0);
  double c = acos(cos_c);
  if (c < 1e-10) {
    nx = 0;
    ny = 0;
    return;
  }
  if (c > M_PI - 1e-10) {
    // Antipode - map to a point outside the unit circle so it's filtered or
    // masked
    nx = 2.0;
    ny = 0.0;
    return;
  }
  double k = c / sin(c);
  nx = k * cos(phi) * sin(dlon) / M_PI;
  ny = k * (cos(phi0) * sin(phi) - sin(phi0) * cos(phi) * cos(dlon)) / M_PI;
}

static bool inverseAzimuthal(double nx, double ny, double lat0, double lon0,
                             double &lat, double &lon) {
  const double D = M_PI / 180.0;
  double phi0 = lat0 * D, lam0 = lon0 * D;
  double x = nx * M_PI, y = ny * M_PI;
  double rho = sqrt(x * x + y * y);
  if (rho > M_PI)
    return false;  // outside the globe disk
  if (rho < 1e-10) {
    lat = lat0;
    lon = lon0;
    return true;
  }
  double c = rho;
  double phi = asin(
      std::clamp(cos(c) * sin(phi0) + y * sin(c) * cos(phi0) / rho, -1.0, 1.0));
  double lam = lam0 + atan2(x * sin(c),
                            rho * cos(phi0) * cos(c) - y * sin(phi0) * sin(c));
  lat = phi / D;
  lon = lam / D;
  while (lon > 180.0) lon -= 360.0;
  while (lon < -180.0) lon += 360.0;
  return true;
}

MapWidget::MapWidget(int x, int y, int w, int h, TextureManager &texMgr,
                     FontManager &fontMgr, NetworkManager &netMgr,
                     std::shared_ptr<HamClockState> state, AppConfig &config)
    : Widget(x, y, w, h),
      texMgr_(texMgr),
      fontMgr_(fontMgr),
      netMgr_(netMgr),
      state_(std::move(state)),
      lastPosUpdateMs_(0),
      lastSatTrackUpdateMs_(0),
      config_(config),
      lastMufUpdateMs_(0),
      wxLastCheckMs_(0),
      lastPropUpdateMs_(0),
      lastPropProj_(""),
      lastPropMapRect_({0, 0, 0, 0}),
      lastPropCenterLon_(-999.0),
      lastMapCenterLon_(-999.0) {
  const char *driver = SDL_GetCurrentVideoDriver();
  LOG_D("MapWidget", "SDL Video Driver: {}", driver ? driver : "unknown");

  // KMSDRM driver on RPi has issues with SDL_RenderGeometry.
  if (driver && strcasecmp(driver, "kmsdrm") == 0) {
    useCompatibilityRenderPath_ = true;
    LOG_D("MapWidget",
          "KMSDRM detected, enabling night overlay compatibility path.");
  }

  // Initialize WxMbProvider and GribCloudProvider
  wxmb_ = std::make_shared<WxMbProvider>(netMgr_);
  gribCloud_ = std::make_shared<GribCloudProvider>(netMgr_);

  // Initialize MapViewMenu
  mapViewMenu_ = std::make_unique<MapViewMenu>(fontMgr);
  setTheme(config.theme);

  // Initialize map rectangle
  recalcMapRect();
}
void MapWidget::setTheme(const std::string &theme) {
  Widget::setTheme(theme);
  if (mapViewMenu_) {
    mapViewMenu_->setTheme(theme);
  }
  // Invalidate tooltip cache to pick up new colors
  if (tooltip_.cachedTexture) {
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
    tooltip_.cachedTexture = nullptr;
    tooltip_.cachedText.clear();
  }
}
void MapWidget::setMetric(bool metric) { Widget::setMetric(metric); }
void MapWidget::onFontChanged() {
  if (mapViewMenu_) {
    mapViewMenu_->onFontChanged();
  }
  // Invalidate tooltip cache to pick up new font metrics
  if (tooltip_.cachedTexture) {
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
    tooltip_.cachedTexture = nullptr;
    tooltip_.cachedText.clear();
  }
}

void MapWidget::resetMap() {
  config_.mapZoom = 1.0;
  config_.mapPanX = 0;
  config_.mapPanY = 0;
  mapVerts_.clear();
  gridDirty_ = true;
  borderDirty_ = true;
  greatCircleDirty_ = true;
  satTrackDirty_ = true;
  asteroidTrackDirty_ = true;
  wxVerts_.clear();
  if (wxmb_)
    wxmb_->invalidate();
}

void MapWidget::showDEMenu(int mx, int my) {
  if (mapViewMenu_->isVisible())
    return;

  double lat, lon;
  if (!screenToLatLon(mx, my, lat, lon)) {
    deMenuVisible_ = false;
    return;
  }

  deMenuVisible_ = true;
  deMenuLat_ = lat;
  deMenuLon_ = lon;

  // Size the menu
  int menuW = 110;
  int menuH = 30;
  deMenuRect_ = {mx, my, menuW, menuH};

  // Boundary check
  if (deMenuRect_.x + deMenuRect_.w > HamClock::LOGICAL_WIDTH)
    deMenuRect_.x -= deMenuRect_.w;
  if (deMenuRect_.y + deMenuRect_.h > HamClock::LOGICAL_HEIGHT)
    deMenuRect_.y -= deMenuRect_.h;
}

MapWidget::~MapWidget() {
  // Signal any in-flight WorkerService ground-track task to exit early rather
  // than dereference the now-dangling predictor_.
  trackAlive_->store(false, std::memory_order_release);
  MemoryMonitor::getInstance().destroyTexture(nightOverlayTexture_);
  MemoryMonitor::getInstance().destroyTexture(propTexture_);
  MemoryMonitor::getInstance().destroyTexture(auroraTexture_);
  MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);
  if (wxFillTex_)
    MemoryMonitor::getInstance().destroyTexture(wxFillTex_);
  if (gribCloudFillTex_)
    MemoryMonitor::getInstance().destroyTexture(gribCloudFillTex_);
}
void MapWidget::recalcMapRect() {
  if (config_.projection == "azimuthal") {
    // Circular projection fits in a square
    int side = std::min(width_, height_);
    mapRect_.w = side;
    mapRect_.h = side;
    mapRect_.x = x_ + (width_ - side) / 2;
    mapRect_.y = y_ + (height_ - side) / 2;
  } else if (config_.projection == "dual_azimuthal") {
    // Two side-by-side circles — use full widget area so overlay code works
    mapRect_.x = x_;
    mapRect_.y = y_;
    mapRect_.w = width_;
    mapRect_.h = height_;
  } else {
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
}

SDL_FPoint MapWidget::latLonToScreen(double lat, double lon) const {
  if (config_.projection == "equirectangular") {
    double dLon = lon - config_.mapCenterLon;
    while (dLon > 180.0) dLon -= 360.0;
    while (dLon < -180.0) dLon += 360.0;
    // Clamp coordinates to [0, 1] range to avoid edge sampling artifacts
    double nx = (dLon + 180.0) / 360.0;
    double ny = (90.0 - lat) / 180.0;
    float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
    float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
    if (config_.mapZoom > 1.0) {
      float cx = mapRect_.x + mapRect_.w * 0.5f;
      float cy = mapRect_.y + mapRect_.h * 0.5f;
      px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
      py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
    }
    return {px, py};
  }
  if (config_.projection == "robinson") {
    double centeredLon = lon - config_.mapCenterLon;
    while (centeredLon > 180.0) centeredLon -= 360.0;
    while (centeredLon < -180.0) centeredLon += 360.0;
    double rnx, rny;
    projectRobinson(lat, centeredLon, rnx, rny);
    float px = static_cast<float>(mapRect_.x + (rnx + 1.0) * 0.5 * mapRect_.w);
    float py = static_cast<float>(mapRect_.y + (1.0 - rny) * 0.5 * mapRect_.h);
    if (config_.mapZoom > 1.0) {
      float cx = mapRect_.x + mapRect_.w * 0.5f;
      float cy = mapRect_.y + mapRect_.h * 0.5f;
      px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
      py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
    }
    return {px, py};
  }
  if (config_.projection == "azimuthal") {
    double deLat = state_ ? state_->deLocation.lat : 0.0;
    double deLon = state_ ? state_->deLocation.lon : 0.0;
    double nx, ny;
    projectAzimuthal(lat, lon, deLat, deLon, nx, ny);

    float scale = std::min(mapRect_.w, mapRect_.h);
    float px = mapRect_.x + (mapRect_.w * 0.5f) + (float)nx * (scale * 0.5f);
    float py = mapRect_.y + (mapRect_.h * 0.5f) - (float)ny * (scale * 0.5f);
    if (config_.mapZoom > 1.0) {
      float cx = mapRect_.x + mapRect_.w * 0.5f;
      float cy = mapRect_.y + mapRect_.h * 0.5f;
      px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
      py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
    }
    return {px, py};
  }
  if (config_.projection == "dual_azimuthal") {
    double deLat = state_ ? state_->deLocation.lat : 0.0;
    double deLon = state_ ? state_->deLocation.lon : 0.0;
    int halfW = mapRect_.w / 2;
    float R = std::min(halfW, mapRect_.h) * 0.5f;
    // Left hemisphere: DE-centered
    float cxL = mapRect_.x + halfW * 0.5f;
    float cy = mapRect_.y + mapRect_.h * 0.5f;
    double nx, ny;
    projectAzimuthal(lat, lon, deLat, deLon, nx, ny);
    float px = cxL + (float)nx * R;
    float py = cy - (float)ny * R;

    // TODO: if px > halfW, it should probably be in the other hemisphere?
    // The existing logic doesn't seem to handle the split well for individual
    // markers.
    if (config_.mapZoom > 1.0) {
      float cx = mapRect_.x + mapRect_.w * 0.5f;
      px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
      py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
    }
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
    double centeredLon = lon - config_.mapCenterLon;
    while (centeredLon > 180.0) centeredLon -= 360.0;
    while (centeredLon < -180.0) centeredLon += 360.0;
    double nx = (centeredLon + 180.0) / 360.0;
    float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
    float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
    if (config_.mapZoom > 1.0) {
      float cx = mapRect_.x + mapRect_.w * 0.5f;
      float cy = mapRect_.y + mapRect_.h * 0.5f;
      px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
      py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
    }
    return {px, py};
  }
  double centeredLon = lon - config_.mapCenterLon;
  while (centeredLon > 180.0) centeredLon -= 360.0;
  while (centeredLon < -180.0) centeredLon += 360.0;
  double nx = (centeredLon + 180.0) / 360.0;
  double ny = (90.0 - lat) / 180.0;
  float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
  float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
  if (config_.mapZoom > 1.0) {
    float cx = mapRect_.x + mapRect_.w * 0.5f;
    float cy = mapRect_.y + mapRect_.h * 0.5f;
    px = (px - cx) * (float)config_.mapZoom + cx + (float)config_.mapPanX;
    py = (py - cy) * (float)config_.mapZoom + cy + (float)config_.mapPanY;
  }
  return {px, py};
}

bool MapWidget::screenToLatLon(int sx, int sy, double &lat, double &lon) const {
  float fx = (float)sx;
  float fy = (float)sy;

  if (config_.mapZoom > 1.0) {
    float cx = mapRect_.x + mapRect_.w * 0.5f;
    float cy = mapRect_.y + mapRect_.h * 0.5f;
    fx = (fx - (float)config_.mapPanX - cx) / (float)config_.mapZoom + cx;
    fy = (fy - (float)config_.mapPanY - cy) / (float)config_.mapZoom + cy;
  }

  if (fx < mapRect_.x || fx > mapRect_.x + mapRect_.w || fy < mapRect_.y ||
      fy > mapRect_.y + mapRect_.h)
    return false;

  if (config_.projection == "robinson") {
    double rnx =
        (static_cast<double>(fx - mapRect_.x) / mapRect_.w) * 2.0 - 1.0;
    double rny =
        1.0 - (static_cast<double>(fy - mapRect_.y) / mapRect_.h) * 2.0;
    if (!inverseRobinson(rnx, rny, lat, lon))
      return false;
    lon += config_.mapCenterLon;
    while (lon > 180.0)
      lon -= 360.0;
    while (lon < -180.0)
      lon += 360.0;
    return true;
  }
  if (config_.projection == "azimuthal") {
    double nx = (static_cast<double>(fx - mapRect_.x) / mapRect_.w) * 2.0 - 1.0;
    double ny = 1.0 - (static_cast<double>(fy - mapRect_.y) / mapRect_.h) * 2.0;
    double deLat = state_ ? state_->deLocation.lat : 0.0;
    double deLon = state_ ? state_->deLocation.lon : 0.0;
    return inverseAzimuthal(nx, ny, deLat, deLon, lat, lon);
  }
  if (config_.projection == "dual_azimuthal") {
    double deLat = state_ ? state_->deLocation.lat : 0.0;
    double deLon = state_ ? state_->deLocation.lon : 0.0;
    int halfW = mapRect_.w / 2;
    float R = std::min(halfW, mapRect_.h) * 0.5f;
    bool rightHalf = (fx >= mapRect_.x + halfW);
    float cx, cy;
    double centerLat, centerLon;
    if (!rightHalf) {
      cx = mapRect_.x + halfW * 0.5f;
      cy = mapRect_.y + mapRect_.h * 0.5f;
      centerLat = deLat;
      centerLon = deLon;
    } else {
      cx = mapRect_.x + halfW + halfW * 0.5f;
      cy = mapRect_.y + mapRect_.h * 0.5f;
      // Antipodal center
      centerLat = -deLat;
      centerLon = deLon + (deLon >= 0.0 ? -180.0 : 180.0);
    }
    double nx = (fx - cx) / R;
    double ny = (cy - fy) / R;
    return inverseAzimuthal(nx, ny, centerLat, centerLon, lat, lon);
  }
  if (config_.projection == "mercator") {
    double nx = static_cast<double>(fx - mapRect_.x) / mapRect_.w;
    double ny = static_cast<double>(fy - mapRect_.y) / mapRect_.h;
    lon = nx * 360.0 - 180.0 + config_.mapCenterLon;
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    constexpr double maxLat = 85.05112878;
    double maxMercY =
        std::log(std::tan(M_PI / 4.0 + (maxLat * M_PI / 180.0) / 2.0));
    double mercY = (0.5 - ny) * 2.0 * maxMercY;
    lat = (2.0 * std::atan(std::exp(mercY)) - M_PI / 2.0) * 180.0 / M_PI;
    return true;
  }

  double nx = static_cast<double>(fx - mapRect_.x) / mapRect_.w;
  double ny = static_cast<double>(fy - mapRect_.y) / mapRect_.h;
  lon = nx * 360.0 - 180.0 + config_.mapCenterLon;
  while (lon > 180.0) lon -= 360.0;
  while (lon < -180.0) lon += 360.0;
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

  // Handle deferred DE menu display for right-click timing
  if (deMenuPending_ && nowMs - rightClickTimeMs_ > 300) {
    showDEMenu(deMenuX_, deMenuY_);
    deMenuPending_ = false;
  }

  // Auto-center on DE if enabled
  if (config_.centerMapOnDe) {
    if (std::abs(config_.mapCenterLon - state_->deLocation.lon) > 0.001) {
      config_.mapCenterLon = state_->deLocation.lon;
    }
  } else if (std::abs(config_.mapCenterLon) > 0.001) {
    config_.mapCenterLon = 0.0;
  }

  // Detect map shift and invalidate mesh/geometry
  if (std::abs(config_.mapCenterLon - lastMapCenterLon_) > 0.001) {
    mapVerts_.clear();
    mapBaseIndices_.clear();
    gridDirty_ = true;
    borderDirty_ = true;
    greatCircleDirty_ = true;
    satTrackDirty_ = true;
    lastMapCenterLon_ = config_.mapCenterLon;
    LOG_D("MapWidget", "Map center shifted to {}, invalidating mesh.",
          config_.mapCenterLon);
  }

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

      WorkerService::getInstance().submitTask([this, alive = trackAlive_] {
        // alive_ is set to false in ~MapWidget() before predictor_ is freed.
        // If this task fires after destruction, exit before touching predictor_.
        if (!alive->load(std::memory_order_acquire))
          return;
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

  // Asteroid ground track update
  if (asteroidProvider_ && state_) {
    const std::string &sel = state_->selectedAsteroidName;
    if (sel != lastSelectedAsteroidName_) {
      lastSelectedAsteroidName_ = sel;
      asteroidTrackDirty_ = true;
      cachedAsteroidTrack_.clear();
      asteroidTrackVerts_.clear();
      asteroidTrackIndices_.clear();
    }
    if (!sel.empty() && asteroidTrackDirty_) {
      OrbitalElements elem = asteroidProvider_->getOrbitalElements(sel);
      if (elem.valid) {
        // Find approach JD for the selected asteroid
        double jd_approach = 0;
        AsteroidData latest = asteroidProvider_->getLatest();
        for (const auto &ast : latest.asteroids) {
          if (ast.name == sel) {
            jd_approach = ast.julianDate;
            break;
          }
        }
        if (jd_approach > 0) {
          cachedAsteroidTrack_ = AsteroidPropagator::computeGroundTrack(
              elem, jd_approach - 0.5, jd_approach + 0.5, 48);
          asteroidTrackDirty_ = false;
        }
      }
    }
  } else if (!cachedAsteroidTrack_.empty()) {
    cachedAsteroidTrack_.clear();
    asteroidTrackDirty_ = true;
    asteroidTrackVerts_.clear();
    asteroidTrackIndices_.clear();
  }

  // Great Circle update (on change)
  if (state_->dxActive) {
    if (state_->deLocation.lat != lastDE_.lat ||
        state_->deLocation.lon != lastDE_.lon ||
        state_->dxLocation.lat != lastDX_.lat ||
        state_->dxLocation.lon != lastDX_.lon ||
        config_.mapZoom != lastZoom_) {
      int baseSegments = 250;
      int segments = static_cast<int>(baseSegments * (1.0 + 0.8 * (config_.mapZoom - 1.0)));
      cachedGreatCircle_ = Astronomy::calculateGreatCirclePath(
          state_->deLocation, state_->dxLocation, segments);
      lastDE_ = state_->deLocation;
      lastDX_ = state_->dxLocation;
      lastZoom_ = config_.mapZoom;
      greatCircleDirty_ = true;
    }
  } else if (!cachedGreatCircle_.empty()) {
    cachedGreatCircle_.clear();
    greatCircleDirty_ = true;
  }

  // Propagation Rotation
  if (config_.propRotation.size() > 1 && config_.rotationIntervalS > 0) {
    uint32_t intervalMs = static_cast<uint32_t>(config_.rotationIntervalS * 1000);
    if (nowMs - lastPropRotateMs_ >= intervalMs || lastPropRotateMs_ == 0) {
      if (lastPropRotateMs_ != 0) {
        propRotationIdx_ = (propRotationIdx_ + 1) % config_.propRotation.size();
      }
      config_.propOverlay = config_.propRotation[propRotationIdx_];
      lastPropRotateMs_ = nowMs;
    }
  } else if (!config_.propRotation.empty()) {
    // If rotation set is defined but has 1 item, ensure it's synced
    if (config_.propOverlay != config_.propRotation[0]) {
      config_.propOverlay = config_.propRotation[0];
      propRotationIdx_ = 0;
    }
  }

  // Propagation Overlay updates (every 15 mins or on change)
  if (config_.propOverlay != PropOverlayType::None) {
    bool changed = (lastPropType_ != config_.propOverlay) ||
                   (lastBand_ != config_.propBand) ||
                   (lastMode_ != config_.propMode) ||
                   (lastPower_ != config_.propPower) ||
                   (lastToa_ != config_.propToa) ||
                   (lastPath_ != config_.propPath);

    // DRAP reads a live data store: poll every 60s (2s until first data
    // arrives)
    uint32_t propInterval = 900000u;
    if (config_.propOverlay == PropOverlayType::Drap) {
      const bool hasDrapData = drapStore_ && drapStore_->get().valid;
      propInterval = hasDrapData ? 60000u : 2000u;
    }
    if (changed || (nowMs - lastPropUpdateMs_ > propInterval)) {
      updatePropagationOverlay();
      lastPropUpdateMs_ = nowMs;
      lastPropType_ = config_.propOverlay;
      lastBand_ = config_.propBand;
      lastMode_ = config_.propMode;
      lastPower_ = config_.propPower;
      lastToa_ = config_.propToa;
      lastPath_ = config_.propPath;
    }
  }

  // WX pressure overlay (check every 10 minutes)
  if (config_.weatherOverlay == WeatherOverlayType::WxMb) {
    if (nowMs - (uint32_t)wxLastCheckMs_ > 600000 || wxLastCheckMs_ == 0) {
      wxLastCheckMs_ = (uint64_t)nowMs;
      wxmb_->update();
    }
  }

  // GFS TCDC cloud overlay (check every 10 minutes — GFS cycles every 6h)
  if (config_.weatherOverlay == WeatherOverlayType::CloudsGrib && gribCloud_) {
    if (gribCloudLastCheckMs_ == 0 ||
        nowMs - (uint32_t)gribCloudLastCheckMs_ > 600000) {
      gribCloudLastCheckMs_ = (uint64_t)nowMs;
      gribCloud_->update();
    }
  }

  // Monthly map texture update
  auto now_for_month = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now_for_month);
  std::tm *tm = std::localtime(&t);
  int month = tm->tm_mon + 1;  // 1-12

  if (month != currentMonth_ || config_.mapStyle != lastStyle_) {
    currentMonth_ = month;
    lastStyle_ = config_.mapStyle;

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
      if (!style.empty() && style != "nasa") {
        LOG_W("MapWidget", "Unknown map style '{}', falling back to 'nasa'",
              style);
      }
      // Note: BMNG-Base is the standard "Blue Marble" look.
      std::snprintf(
          url, sizeof(url),
          "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/"
          "bmng/bmng-base/%s/world.2004%02d.3x5400x2700.jpg",
          kMonthNames[month - 1], month);
    }

    LOG_I("MapWidget", "Starting async fetch for {}", url);
    netMgr_.fetchAsync(
        url,
        [url_str = std::string(url)](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for {}", data.size(),
                  url_str);
            SDL_Event ev;
            SDL_zero(ev);
            ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_MAP_IMAGE_READY;
            ev.user.code = 0;  // Day map
            ev.user.data1 = new std::string(std::move(data));
            SDL_PushEvent(&ev);
          } else {
            LOG_E("MapWidget", "Fetch failed or empty for {}", url_str);
          }
        },
        86400 * 30);  // Cache for a month

    const char *nightUrl =
        "https://eoimages.gsfc.nasa.gov/images/imagerecords/"
        "79000/79765/dnb_land_ocean_ice.2012.3600x1800.jpg";
    LOG_I("MapWidget", "Starting async fetch for Night Lights");
    netMgr_.fetchAsync(
        nightUrl,
        [nightUrlStr = std::string(nightUrl)](std::string data) {
          if (!data.empty()) {
            LOG_I("MapWidget", "Received {} bytes for Night Lights",
                  data.size());
            SDL_Event ev;
            SDL_zero(ev);
            ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_MAP_IMAGE_READY;
            ev.user.code = 1;  // Night map
            ev.user.data1 = new std::string(std::move(data));
            SDL_PushEvent(&ev);
          } else {
            LOG_E("MapWidget", "Night Lights fetch failed for {}", nightUrlStr);
          }
        },
        86400 * 365);  // Cache for a year
  }

  if (config_.propOverlay != PropOverlayType::None) {
    if (iono_ && iono_->hasData()) {
      uint32_t lastUp = iono_->getLastUpdateMs();
      if (lastUp != lastMufUpdateMs_) {
        updatePropagationOverlay();
        lastMufUpdateMs_ = lastUp;
      }
    }
  }
}

void MapWidget::render(SDL_Renderer *renderer) {
  cachedRenderer_ = renderer;
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
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
    bool projChanged = (lastProjection_ != config_.projection);
    if (projChanged) {
      lastProjection_ = config_.projection;
      borderDirty_ = true;
      gridDirty_ = true;
      greatCircleDirty_ = true;
      satTrackDirty_ = true;
      asteroidTrackDirty_ = true;
      // Invalidate overlay meshes for the new projection
      shadowVerts_.clear();
      lightVerts_.clear();
      nightIndices_.clear();
      nightLightIndices_.clear();
      auroraVerts_.clear();
      propVerts_.clear();
      // Clear WX GPU buffers — will be re-projected on next getSegments() call.
      wxVerts_.clear();
      wxIndices_.clear();
      if (wxmb_)
        wxmb_->invalidate();
    }

    if (config_.projection != "equirectangular" ||
        std::abs(config_.mapCenterLon) > 0.001) {
      // Draw using mesh to support Robinson, Mercator, or Azimuthal warping
      // Low-memory mode: reduce mesh density on KMSDRM
      const bool isAz = (config_.projection == "azimuthal" ||
                         config_.projection == "dual_azimuthal");
      const int gridW =
          useCompatibilityRenderPath_ ? (isAz ? 96 : 48) : (isAz ? 192 : 96);
      const int gridH =
          useCompatibilityRenderPath_ ? (isAz ? 48 : 24) : (isAz ? 96 : 48);
      bool needsMeshUpdate =
          mapVerts_.empty() ||
          (mapVerts_.size() != (size_t)(gridW + 1) * (gridH + 1)) ||
          projChanged;

      if (needsMeshUpdate) {
        mapVerts_.resize((gridW + 1) * (gridH + 1));
        if (config_.projection == "azimuthal") {
          // Screen-space grid for Azimuthal to avoid antipode singularities
          double deLat = state_ ? state_->deLocation.lat : 0.0;
          double deLon = state_ ? state_->deLocation.lon : 0.0;
          float cx = mapRect_.x + mapRect_.w * 0.5f;
          float cy = mapRect_.y + mapRect_.h * 0.5f;
          float R = std::min(mapRect_.w, mapRect_.h) * 0.5f;

          for (int j = 0; j <= gridH; ++j) {
            float sy = mapRect_.y + (float)j * mapRect_.h / gridH;
            for (int i = 0; i <= gridW; ++i) {
              float sx = mapRect_.x + (float)i * mapRect_.w / gridW;
              int idx = j * (gridW + 1) + i;
              double nx = (sx - cx) / R;
              double ny = (cy - sy) / R;

              if (config_.mapZoom > 1.0) {
                nx = (nx * R - (float)config_.mapPanX) / (float)config_.mapZoom / R;
                ny = (ny * R + (float)config_.mapPanY) / (float)config_.mapZoom / R;
              }

              double lat, lon;
              if (inverseAzimuthal(nx, ny, deLat, deLon, lat, lon)) {
                float u = static_cast<float>((lon + 180.0) / 360.0);
                float v = static_cast<float>((90.0 - lat) / 180.0);
                mapVerts_[idx] = {{sx, sy}, {255, 255, 255, 255}, {u, v}};
              } else {
                mapVerts_[idx] = {{sx, sy}, {0, 0, 0, 0}, {0, 0}};
              }
            }
          }
        } else if (config_.projection == "dual_azimuthal") {
          // Screen-space grid for dual azimuthal (two hemispheres)
          double deLat = state_ ? state_->deLocation.lat : 0.0;
          double deLon = state_ ? state_->deLocation.lon : 0.0;
          double antiLat = -deLat;
          double antiLon = deLon + (deLon >= 0.0 ? -180.0 : 180.0);
          int halfW = mapRect_.w / 2;
          float R = std::min(halfW, mapRect_.h) * 0.5f;
          float cxL = mapRect_.x + halfW * 0.5f;
          float cxR = mapRect_.x + halfW + halfW * 0.5f;
          float cy = mapRect_.y + mapRect_.h * 0.5f;

          for (int j = 0; j <= gridH; ++j) {
            float sy = mapRect_.y + (float)j * mapRect_.h / gridH;
            for (int i = 0; i <= gridW; ++i) {
              float sx = mapRect_.x + (float)i * mapRect_.w / gridW;
              int idx = j * (gridW + 1) + i;
              double lat, lon;
              bool valid;
              if (sx < mapRect_.x + halfW) {
                double nx = (sx - cxL) / R;
                double ny = (cy - sy) / R;
                if (config_.mapZoom > 1.0) {
                  nx = (nx * R - (float)config_.mapPanX) / (float)config_.mapZoom / R;
                  ny = (ny * R + (float)config_.mapPanY) / (float)config_.mapZoom / R;
                }
                valid = inverseAzimuthal(nx, ny, deLat, deLon, lat, lon);
              } else {
                double nx = (sx - cxR) / R;
                double ny = (cy - sy) / R;
                if (config_.mapZoom > 1.0) {
                  nx = (nx * R - (float)config_.mapPanX) / (float)config_.mapZoom / R;
                  ny = (ny * R + (float)config_.mapPanY) / (float)config_.mapZoom / R;
                }
                valid = inverseAzimuthal(nx, ny, antiLat, antiLon, lat, lon);
              }
              if (valid) {
                // Ensure UV coordinates are strictly within [0, 1] to avoid texture wrapping artifacts
                float u = std::clamp(static_cast<float>((lon + 180.0) / 360.0), 0.0f, 1.0f);
                float v = std::clamp(static_cast<float>((90.0 - lat) / 180.0), 0.0f, 1.0f);
                mapVerts_[idx] = {{sx, sy}, {255, 255, 255, 255}, {u, v}};
              } else {
                mapVerts_[idx] = {{sx, sy}, {0, 0, 0, 0}, {0, 0}};
              }
            }
          }
        } else {
          // Standard equirectangular-to-warped mesh
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
        }

        // Build base-map index buffer with seam culling.
        // Uses screen-x detection (not UV) because mapVerts_ is in lat/lon
        // space and seam-crossing triangles have a full-width screen-x jump.
        // Separate from nightIndices_ which is used by the screen-space night
        // overlay mesh and is managed by renderNightOverlay().
        if (mapBaseIndices_.size() != (size_t)(gridW * gridH * 6)) {
          mapBaseIndices_.clear();
          mapBaseIndices_.reserve(gridW * gridH * 6);
          for (int j = 0; j < gridH; ++j) {
            for (int i = 0; i < gridW; ++i) {
              int p0 = j * (gridW + 1) + i;
              int p1 = p0 + 1;
              int p2 = (j + 1) * (gridW + 1) + i;
              int p3 = p2 + 1;

              bool wrap = false;
              if (config_.projection == "azimuthal" ||
                  config_.projection == "dual_azimuthal" ||
                  std::abs(config_.mapCenterLon) > 0.001) {
                float x0 = mapVerts_[p0].position.x;
                float x1 = mapVerts_[p1].position.x;
                float x2 = mapVerts_[p2].position.x;
                float x3 = mapVerts_[p3].position.x;
                float threshold = mapRect_.w * 0.5f;
                if (std::abs(x0 - x1) > threshold ||
                    std::abs(x0 - x2) > threshold ||
                    std::abs(x1 - x3) > threshold) {
                  wrap = true;
                }
              }

              if (!wrap) {
                mapBaseIndices_.push_back(p0);
                mapBaseIndices_.push_back(p1);
                mapBaseIndices_.push_back(p2);
                mapBaseIndices_.push_back(p2);
                mapBaseIndices_.push_back(p1);
                mapBaseIndices_.push_back(p3);
              }
            }
          }
        }
      }

      SDL_RenderGeometry(renderer, mapTex, mapVerts_.data(),
                         (int)mapVerts_.size(), mapBaseIndices_.data(),
                         (int)mapBaseIndices_.size());
    } else {
      SDL_RenderCopy(renderer, mapTex, nullptr, &mapRect_);
    }
  }

  // Star field: drawn AFTER the map so stars are visible in the black corner
  // areas of Azimuthal and Robinson projections rather than underneath the map
  // blit (which overwrites anything drawn before it in those corners).
  renderStarField(renderer);

  renderWxMbOverlay(renderer);
  renderGribCloudOverlay(renderer);
  renderMufRtOverlay(renderer);
  renderNightOverlay(renderer);
  renderPropagationOverlay(renderer);
  
  // Overlays (clipped to mapRect_)
  SDL_Rect clipRect = mapRect_;
  SDL_RenderSetClipRect(renderer, &clipRect);

  // Render global references and boundaries
  renderAuroraOverlay(renderer);
  if (config_.showBorders)
    renderCountryBorders(renderer);
  renderGridOverlay(renderer);

  // Render paths and dynamic markers
  renderGreatCircle(renderer);

  renderSatellite(renderer);
  renderAsteroidOverlay(renderer);
  renderSpotOverlay(renderer);
  renderDXClusterSpots(renderer);
  renderADIFPins(renderer);
  renderONTASpots(renderer);
  renderBeacons(renderer);

  SDL_RenderSetClipRect(renderer, nullptr);

  renderMarker(renderer, state_->deLocation.lat, state_->deLocation.lon,
               themes.accent.r, themes.accent.g, themes.accent.b);
  if (state_->dxActive) {
    uint8_t r = themes.success.r;
    uint8_t g = themes.success.g;
    uint8_t b = themes.success.b;

    if (state_->dxFreqKhz > 0) {
      int bi = freqToBandIndex(state_->dxFreqKhz);
      if (bi >= 0) {
        r = kBands[bi].color.r;
        g = kBands[bi].color.g;
        b = kBands[bi].color.b;
      }
    }

    renderMarker(renderer, state_->dxLocation.lat, state_->dxLocation.lon,
                 r, g, b, MarkerShape::CircleWithDot);

    // If manual map selection (no callsign), show the grid square label
    if (state_->dxCallsign.empty() && !state_->dxGrid.empty()) {
      SDL_FPoint sp = latLonToScreen(state_->dxLocation.lat, state_->dxLocation.lon);
      SDL_Color color = {r, g, b, 255};
      fontMgr_.catalog()->drawText(renderer, state_->dxGrid, static_cast<int>(sp.x) + 8,
                                   static_cast<int>(sp.y), color, FontStyle::Tiny,
                                   /*centered=*/false, /*rightAlign=*/false, /*vertCentered=*/true);
    }
  }

  renderMarker(renderer, sunLat_, sunLon_, themes.warning.r, themes.warning.g,
               0, MarkerShape::Circle, true);

  if (config_.projection == "azimuthal" ||
      config_.projection == "dual_azimuthal") {
    renderAzimuthalMask(renderer);
  }

  // Render UI HUD elements and legends on top
  renderProjectionSelect(renderer);
  renderRssButton(renderer);
  renderOverlayInfo(renderer);
  renderLegend(renderer);
  renderWxMbLegend(renderer);
  renderCloudLegend(renderer);

  renderCalendarAlert(renderer);

  // Note: MapViewMenu is rendered via renderModal() in the centralized modal
  // pass, not here. This prevents clipping to the map pane bounds.

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);
}

void MapWidget::renderTooltipLayer(SDL_Renderer *renderer) {
  renderTooltip(renderer);
  renderDeMenu(renderer);
}

void MapWidget::renderDeMenu(SDL_Renderer *renderer) {
  if (!deMenuVisible_)
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &deMenuRect_);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &deMenuRect_);

  // Text
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "Set DE Here", deMenuRect_.x + deMenuRect_.w / 2,
                deMenuRect_.y + deMenuRect_.h / 2, themes.text, FontStyle::Fast,
                true, false, true);
}

// DRAP absorption color: SDL_PIXELFORMAT_RGBA32 little-endian packing
// pixels[i] = (a << 24) | (b << 16) | (g << 8) | r
static uint32_t drapColor(float mhz, const std::string &variant) {
  float t = std::min(mhz / 30.0f, 1.0f);
  SDL_Color c = MapWidget::getPropColor(PropOverlayType::Drap, t, variant);
  uint8_t a = (mhz < 0.1f) ? 0 : (mhz < 5.0f ? 80 : (mhz < 15.0f ? 160 : 200));
  return (static_cast<uint32_t>(a) << 24) |
         (static_cast<uint32_t>(c.b) << 16) |
         (static_cast<uint32_t>(c.g) << 8) |
         static_cast<uint32_t>(c.r);
}

void MapWidget::updatePropagationOverlay() {
  if (config_.propOverlay == PropOverlayType::None) {
    return;
  }

  // DRAP grid: read synchronously from drapStore_, upload texture on main
  // thread
  if (config_.propOverlay == PropOverlayType::Drap) {
    if (!drapStore_)
      return;
    auto grid = drapStore_->get();
    LOG_D("MapWidget", "DRAP update: valid={} rows={} cols={} cells={}",
          grid.valid, grid.rows, grid.cols, (int)grid.cells.size());
    if (!grid.valid || grid.cells.empty() || grid.cols <= 0 || grid.rows <= 0) {
      // Clear stale texture from previous overlay so nothing is rendered
      if (propTexture_)
        MemoryMonitor::getInstance().destroyTexture(propTexture_);
      if (auroraTexture_)
        MemoryMonitor::getInstance().destroyTexture(auroraTexture_);
      if (nightOverlayTexture_)
        MemoryMonitor::getInstance().destroyTexture(nightOverlayTexture_);
      propTexture_ = nullptr;
      auroraTexture_ = nullptr;
      nightOverlayTexture_ = nullptr;
      return;
    }

    SDL_Renderer *renderer = cachedRenderer_;
    if (!renderer)
      return;

    // Recreate propTexture_ if dimensions differ
    if (propTexture_) {
      int texW = 0, texH = 0;
      SDL_QueryTexture(propTexture_, nullptr, nullptr, &texW, &texH);
      if (texW != grid.cols || texH != grid.rows) {
        MemoryMonitor::getInstance().destroyTexture(propTexture_);
        propTexture_ = nullptr;
      }
    }
    if (!propTexture_) {
      propTexture_ =
          SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                            SDL_TEXTUREACCESS_STATIC, grid.cols, grid.rows);
      SDL_SetTextureBlendMode(propTexture_, SDL_BLENDMODE_BLEND);
    }
    if (!propTexture_)
      return;

    std::vector<uint32_t> pixels(grid.cells.size());
    for (int i = 0; i < (int)grid.cells.size(); i++)
      pixels[i] = drapColor(grid.cells[i], config_.propColormap);
    SDL_UpdateTexture(propTexture_, nullptr, pixels.data(),
                      grid.cols * sizeof(uint32_t));
    return;
  }

  // Heatmap data comes from ReachProvider via onPropDataReady() callback —
  // not from PropEngine.  Skip the engine computation entirely.
  if (config_.propOverlay == PropOverlayType::Heatmap) {
    // If we just switched TO Heatmap, destroy the stale texture from the
    // previous overlay so nothing renders until fresh heatmap data arrives.
    if (lastPropType_ != PropOverlayType::Heatmap && propTexture_) {
      MemoryMonitor::getInstance().destroyTexture(propTexture_);
      propTexture_ = nullptr;
    }
    return;
  }

  PropPathParams params;
  params.txLat = state_->deLocation.lat;
  params.txLon = state_->deLocation.lon;

  auto getMhz = [](const std::string &band) -> double {
    if (band == "80m")
      return 3.5;
    if (band == "60m")
      return 5.3;
    if (band == "40m")
      return 7.0;
    if (band == "30m")
      return 10.1;
    if (band == "20m")
      return 14.1;
    if (band == "17m")
      return 18.1;
    if (band == "15m")
      return 21.1;
    if (band == "12m")
      return 24.9;
    if (band == "10m")
      return 28.4;
    if (band == "6m")
      return 50.1;
    return 14.1;
  };

  params.mhz = getMhz(config_.propBand);
  params.watts = config_.propPower;
  params.mode = config_.propMode;
  params.toa = (int)config_.propToa;
  params.path = config_.propPath;

  SolarData sw{};
  if (solar_) {
    sw = solar_->get();
  }

  // outputType: 0=MUF, 1=Reliability, 2=TOA
  int outputType = 0;
  if (config_.propOverlay == PropOverlayType::Reliability) {
    outputType = 1;
  } else if (config_.propOverlay == PropOverlayType::Toa) {
    outputType = 2;
  }

  PropOverlayType overlayType = config_.propOverlay;

  // MUF (RT) uses real-time ionosonde data; VOACAP/Reliability use solar models
  std::shared_ptr<IonosondeProvider> provider =
      (overlayType == PropOverlayType::Muf) ? iono_ : nullptr;

  WorkerService::getInstance().submitTask(
      [params, sw, provider, outputType, overlayType]() {
        auto grid =
            PropEngine::generateGrid(params, sw, provider.get(), outputType);

        auto *result = new std::vector<float>(std::move(grid));
        SDL_Event event;
        SDL_zero(event);
        event.type = HamClock::AE_BASE_EVENT + HamClock::AE_PROP_DATA_READY;
        event.user.code = static_cast<int>(overlayType);
        event.user.data1 = result;
        SDL_PushEvent(&event);
      });
}

void MapWidget::onPropDataReady(PropOverlayType type,
                                const std::vector<float> &grid) {
  if (grid.size() != PropEngine::MAP_W * PropEngine::MAP_H)
    return;

  lastPropType_ = type;
  lastPropGrid_ = grid;

  // Use the renderer cached during the last render() call.
  SDL_Renderer *renderer = cachedRenderer_;
  if (!renderer)
    return;

  if (!propTexture_) {
    propTexture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STATIC,
                                     PropEngine::MAP_W, PropEngine::MAP_H);
    if (!propTexture_)
      return;
    SDL_SetTextureBlendMode(propTexture_, SDL_BLENDMODE_BLEND);
  }

  std::vector<uint32_t> pixels(grid.size());
  float maxVal;
  if (type == PropOverlayType::Reliability)
    maxVal = 100.0f;
  else if (type == PropOverlayType::Toa)
    maxVal = 40.0f;
  else if (type == PropOverlayType::Heatmap)
    maxVal = 1.0f;  // ReachProvider normalizes grid to 0..1
  else
    maxVal = 50.0f;  // MUF

  for (size_t i = 0; i < grid.size(); ++i) {
    float val = grid[i];
    float t = val / maxVal;
    t = std::max(0.0f, std::min(t, 1.0f));

    SDL_Color c = getPropColor(type, t, config_.propColormap);
    uint8_t r = c.r;
    uint8_t g = c.g;
    uint8_t b = c.b;

    uint8_t a = (val > 0.1f) ? 200 : 0;
    pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }

  SDL_UpdateTexture(propTexture_, nullptr, pixels.data(),
                    PropEngine::MAP_W * sizeof(uint32_t));
}

void MapWidget::forcePropUpdate() {
  if (lastPropGrid_.empty())
    return;
  onPropDataReady(lastPropType_, lastPropGrid_);
}

SDL_Color MapWidget::getPropColor(PropOverlayType type, float t,
                                 const std::string &variant) {
  t = std::max(0.0f, std::min(t, 1.0f));
  uint8_t r = 0, g = 0, b = 0;
  bool vibrant = (variant == "vibrant");

  if (variant == "custom") {
    const auto &ovr = ConfigManager::instance().getConfig().colorOverrides;
    auto getOvrColor = [&](const std::string &key, SDL_Color fallback) {
      auto it = ovr.find(key);
      return (it != ovr.end()) ? it->second : fallback;
    };

    SDL_Color c0 = getOvrColor("prop_color_0", {150, 0, 0, 255});
    SDL_Color c25 = getOvrColor("prop_color_25", {255, 150, 0, 255});
    SDL_Color c50 = getOvrColor("prop_color_50", {255, 255, 0, 255});
    SDL_Color c75 = getOvrColor("prop_color_75", {0, 255, 255, 255});
    SDL_Color c100 = getOvrColor("prop_color_100", {255, 255, 255, 255});

    SDL_Color ca, cb;
    float f;
    if (t < 0.25f) {
      ca = c0; cb = c25; f = t / 0.25f;
    } else if (t < 0.5f) {
      ca = c25; cb = c50; f = (t - 0.25f) / 0.25f;
    } else if (t < 0.75f) {
      ca = c50; cb = c75; f = (t - 0.5f) / 0.25f;
    } else {
      ca = c75; cb = c100; f = (t - 0.75f) / 0.25f;
    }
    r = (uint8_t)(ca.r + f * (cb.r - ca.r));
    g = (uint8_t)(ca.g + f * (cb.g - ca.g));
    b = (uint8_t)(ca.b + f * (cb.b - ca.b));
    return {r, g, b, 255};
  }

  if (type == PropOverlayType::Reliability || type == PropOverlayType::Voacap) {
    if (vibrant) {
      // Vibrant Reliability: Red -> Yellow -> Green -> Cyan -> White
      if (t < 0.25f) {
        float f = t / 0.25f;
        r = (uint8_t)(150 + f * 105);
        g = (uint8_t)(f * 150);
        b = 0;
      } else if (t < 0.5f) {
        float f = (t - 0.25f) / 0.25f;
        r = 255;
        g = (uint8_t)(150 + f * 105);
        b = 0;
      } else if (t < 0.75f) {
        float f = (t - 0.5f) / 0.25f;
        r = (uint8_t)(255 * (1.0f - f));
        g = 255;
        b = (uint8_t)(f * 255);
      } else {
        float f = (t - 0.75f) / 0.25f;
        r = (uint8_t)(f * 255);
        g = 255;
        b = 255;
      }
    } else {
      // Muted Reliability: Grey -> Yellow -> Green
      if (t < 0.5f) {
        float f = t / 0.5f;
        r = (uint8_t)(100 + f * 155);
        g = (uint8_t)(100 + f * 155);
        b = 100;
      } else {
        float f = (t - 0.5f) / 0.5f;
        r = (uint8_t)(100 * (1.0f - f));
        g = 255;
        b = (uint8_t)(100 * (1.0f - f));
      }
    }
  } else if (type == PropOverlayType::Toa) {
    if (vibrant) {
      if (t < 0.5f) {
        float f = t * 2.0f;
        r = (uint8_t)(f * 255);
        g = 255;
        b = 0;
      } else {
        float f = (t - 0.5f) * 2.0f;
        r = 255;
        g = (uint8_t)((1.0f - f) * 255);
        b = 0;
      }
    } else {
      if (t < 0.5f) {
        float f = t * 2.0f;
        r = (uint8_t)(f * 255.0f);
        g = 200;
        b = 0;
      } else {
        float f = (t - 0.5f) * 2.0f;
        r = 255;
        g = (uint8_t)((1.0f - f) * 200.0f);
        b = 0;
      }
    }
  } else if (type == PropOverlayType::Heatmap) {
    if (vibrant) {
      if (t < 0.25f) {
        float f = t / 0.25f;
        r = (uint8_t)(f * 255);
        g = 0;
        b = (uint8_t)((1.0f - f) * 100);
      } else if (t < 0.5f) {
        float f = (t - 0.25f) / 0.25f;
        r = 255;
        g = (uint8_t)(f * 150);
        b = 0;
      } else if (t < 0.75f) {
        float f = (t - 0.5f) / 0.25f;
        r = 255;
        g = (uint8_t)(150 + f * 105);
        b = (uint8_t)(f * 100);
      } else {
        float f = (t - 0.75f) / 0.25f;
        r = 255;
        g = 255;
        b = (uint8_t)(100 + f * 155);
      }
    } else {
      if (t < 0.25f) {
        float f = t / 0.25f;
        r = (uint8_t)(128 + f * 127);
        g = 0;
        b = (uint8_t)(128 * (1.0f - f));
      } else if (t < 0.5f) {
        float f = (t - 0.25f) / 0.25f;
        r = 255;
        g = (uint8_t)(f * 128);
        b = 0;
      } else if (t < 0.75f) {
        float f = (t - 0.5f) / 0.25f;
        r = 255;
        g = (uint8_t)(128 + f * 127);
        b = 0;
      } else {
        float f = (t - 0.75f) / 0.25f;
        r = 255;
        g = 255;
        b = (uint8_t)(f * 255);
      }
    }
  } else if (type == PropOverlayType::Drap) {
    if (vibrant) {
      if (t < 0.33f) { // yellow
        r = 255; g = 255; b = 0;
      } else if (t < 0.66f) { // orange
        r = 255; g = 140; b = 0;
      } else { // red
        r = 220; g = 50; b = 50;
      }
    } else {
      if (t < 0.33f) {
        r = 200; g = 200; b = 50;
      } else if (t < 0.66f) {
        r = 200; g = 120; b = 50;
      } else {
        r = 180; g = 50; b = 50;
      }
    }
  } else {
    // Jet/Turbo style for MUF and others
    if (vibrant) {
      if (t < 0.2f) {
        float f = t / 0.2f;
        b = (uint8_t)(128 + f * 127);
        g = 0;
        r = 0;
      } else if (t < 0.4f) {
        float f = (t - 0.2f) / 0.2f;
        b = 255;
        g = (uint8_t)(f * 255);
      } else if (t < 0.6f) {
        float f = (t - 0.4f) / 0.2f;
        g = 255;
        b = (uint8_t)((1.0f - f) * 255);
      } else if (t < 0.8f) {
        float f = (t - 0.6f) / 0.2f;
        r = (uint8_t)(f * 255);
        g = 255;
      } else {
        float f = (t - 0.8f) / 0.2f;
        r = 255;
        g = (uint8_t)((1.0f - f) * 255);
      }
    } else {
      if (t < 0.25f) {
        float f = t / 0.25f;
        b = 255;
        g = (uint8_t)(f * 255.0f);
      } else if (t < 0.5f) {
        float f = (t - 0.25f) / 0.25f;
        g = 255;
        b = (uint8_t)((1.0f - f) * 255.0f);
      } else if (t < 0.75f) {
        float f = (t - 0.5f) / 0.25f;
        g = 255;
        r = (uint8_t)(f * 255.0f);
      } else {
        float f = (t - 0.75f) / 0.25f;
        r = 255;
        g = (uint8_t)((1.0f - f) * 255.0f);
      }
    }
  }

  return {r, g, b, 255};
}

void MapWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcMapRect();
  if (nightOverlayTexture_) {
    MemoryMonitor::getInstance().destroyTexture(nightOverlayTexture_);
  }
  // Invalidate all cached geometry that depends on screen coordinates
  gridDirty_ = true;
  borderDirty_ = true;
  greatCircleDirty_ = true;
  satTrackDirty_ = true;
  mapVerts_.clear();  // Also force map mesh regen
  auroraVerts_.clear();  // Force aurora mesh regen (positions depend on mapRect_)
}

// --- Tooltip Rendering ---


// --- Semantic API ---

bool MapWidget::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onKeyDown(key, mod);
  }
  return false;
}

bool MapWidget::onTextInput(const char *text) {
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onTextInput(text);
  }
  return false;
}

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

void MapWidget::onMapImageReady(bool night, std::string &&data) {
  std::lock_guard<std::mutex> lock(mapDataMutex_);
  if (night) {
    pendingNightMapData_ = std::move(data);
  } else {
    pendingMapData_ = std::move(data);
  }
}

