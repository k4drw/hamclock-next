#ifdef _WIN32
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#endif
#include "MapWidget.h"
#include "../core/AsteroidPropagator.h"
#include "../core/Astronomy.h"
#include "../core/StarCatalog.h"
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
#include "../core/CountryGrid.h"
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

bool MapWidget::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  // Pass through to menu if visible
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onMouseUp(mx, my, mod, clicks);
  }

  if (deMenuVisible_) {
    SDL_Point pt = {mx, my};
    if (SDL_PointInRect(&pt, &deMenuRect_)) {
      state_->deLocation = {deMenuLat_, deMenuLon_};
      state_->deGrid = Astronomy::latLonToGrid(deMenuLat_, deMenuLon_);
      // Persist to config
      config_.lat = deMenuLat_;
      config_.lon = deMenuLon_;
      config_.grid = state_->deGrid;
      if (onConfigChanged_)
        onConfigChanged_();
    }
    deMenuVisible_ = false;
    return true;
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
      recalcMapRect();  // projection change may alter mapRect_ aspect ratio
      mapLoaded_ = false;
      currentMonth_ = 0;  // Trigger month update
      greatCircleDirty_ = true;
      satTrackDirty_ = true;
      gridDirty_ = true;
      mapVerts_.clear();
      shadowVerts_.clear();  // force night overlay recompute for new projection
      lightVerts_.clear();
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

    // Save as map-click state and clear any panel-driven selection
    state_->mapDxLocation = state_->dxLocation;
    state_->mapDxGrid = state_->dxGrid;
    state_->mapDxActive = true;
    state_->dxCallsign.clear();
    state_->dxFreqKhz = 0.0;

    // Clear both panel selections so they don't conflict
    if (dxcStore_)
      dxcStore_->clearSelection();
    if (activityStore_) {
      auto ad = activityStore_->get();
      ad.hasSelection = false;
      activityStore_->set(ad);
    }
  }

  return true;
}

bool MapWidget::onRightClick(int mx, int my, Uint16 /*mod*/) {
  if (mapViewMenu_->isVisible())
    return false;

  double lat, lon;
  if (!screenToLatLon(mx, my, lat, lon)) {
    deMenuVisible_ = false;
    return false;
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

  return true;
}

bool MapWidget::onMouseWheel(int scrollY) {
  if (mapViewMenu_->isVisible()) {
    return mapViewMenu_->onMouseWheel(scrollY);
  }
  return false;
}

void MapWidget::onMouseMove(int mx, int my) {
  // Named-star hover: check BEFORE screenToLatLon so that stars right at the
  // projection boundary are still reachable even if the mouse is 1-2 px inside
  // the oval edge. The inMap test uses the STAR's position (not the mouse).
  const std::string &proj = config_.projection;
  if ((proj == "azimuthal" || proj == "robinson") &&
      mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    double GMST_deg = Astronomy::calculateGST(std::chrono::system_clock::now()) * 15.0;
    // Azimuthal circle parameters (unused for robinson).
    float az_cx = 0, az_cy = 0, az_R2 = 0;
    if (proj == "azimuthal") {
      az_cx = mapRect_.x + mapRect_.w * 0.5f;
      az_cy = mapRect_.y + mapRect_.h * 0.5f;
      float R = std::min(mapRect_.w, mapRect_.h) * 0.5f;
      az_R2 = R * R;
    }
    constexpr int kStarHitR = 6;
    for (std::size_t i = 0; i < kBrightStarsCount; ++i) {
      const StarEntry &star = kBrightStars[i];
      if (!star.name)
        continue;
      double ra_frac = std::fmod((star.ra_deg - GMST_deg) / 360.0 + 10.0, 1.0);
      int sx = x_ + static_cast<int>(ra_frac * width_);
      int sy = y_ + static_cast<int>((90.0f - star.dec_deg) / 180.0f * height_);
      int ddx = mx - sx, ddy = my - sy;
      if (ddx * ddx + ddy * ddy > kStarHitR * kStarHitR)
        continue;
      // Check that the star itself is NOT inside the visible projection.
      bool starInMap = false;
      if (proj == "azimuthal") {
        float fdx = sx - az_cx, fdy = sy - az_cy;
        starInMap = (fdx * fdx + fdy * fdy) <= az_R2;
      } else {
        double sLat, sLon;
        if (screenToLatLon(sx, sy, sLat, sLon)) {
          SDL_FPoint fwd = latLonToScreen(sLat, sLon);
          starInMap = (std::abs(fwd.x - sx) <= 2.0f && std::abs(fwd.y - sy) <= 2.0f);
        }
      }
      if (!starInMap) {
        tooltip_.text = star.name;
        tooltip_.x = mx;
        tooltip_.y = my;
        tooltip_.visible = true;
        tooltip_.timestamp = SDL_GetTicks();
        return;
      }
    }
  }

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
    tip = state_->dxCallsign.empty() ? "DX" : "DX: " + state_->dxCallsign;
    int crow = std::clamp((int)((state_->dxLocation.lat + 90.0) * 2.0), 0, 359);
    int ccol = std::clamp((int)((state_->dxLocation.lon + 180.0) * 2.0), 0, 719);
    uint16_t cId = COUNTRY_GRID[crow][ccol];
    if (cId > 0 && cId < NUM_COUNTRIES)
      tip += "\n" + std::string(COUNTRY_NAMES[cId]);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "\n%.2f\xc2\xb0%c  %.2f\xc2\xb0%c",
                  std::fabs(state_->dxLocation.lat),
                  state_->dxLocation.lat >= 0 ? 'N' : 'S',
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

  // 5. Check ONTA selected spot only
  if (tip.empty() && activityStore_) {
    ActivityData ads = activityStore_->get();
    if (ads.hasSelection) {
      const auto &sel = ads.selectedSpot;
      // Resolve lat/lon: use selectedSpot coords, or fall back to ontaSpots
      // list
      double sLat = sel.lat, sLon = sel.lon;
      if (sLat == 0.0 && sLon == 0.0) {
        for (const auto &s : ads.ontaSpots) {
          if (s.call == sel.call && s.ref == sel.ref &&
              (s.lat != 0.0 || s.lon != 0.0)) {
            sLat = s.lat;
            sLon = s.lon;
            break;
          }
        }
      }
      if (sLat != 0.0 && screenDist(sLat, sLon) < kHitRadius) {
        tip = sel.call;
        char buf[128];
        int bi = freqToBandIndex(sel.freqKhz);
        std::snprintf(buf, sizeof(buf), " %.1f kHz", sel.freqKhz);
        tip += buf;
        if (bi >= 0)
          tip += std::string(" (") + kBands[bi].name + ")";
        if (!sel.mode.empty())
          tip += " " + sel.mode;
        tip += "\n" + sel.program + ": " + sel.ref;
      }
    }
  }

  // 6. Check asteroid
  if (tip.empty() && asteroidProvider_ &&
      !state_->selectedAsteroidName.empty() && !cachedAsteroidTrack_.empty()) {
    bool hit = false;
    // Check icon midpoint
    size_t mid = cachedAsteroidTrack_.size() / 2;
    if (screenDist(cachedAsteroidTrack_[mid].lat,
                   cachedAsteroidTrack_[mid].lon) < kHitRadius + 4) {
      hit = true;
    } else {
      // Check near ground track line
      for (size_t i = 1; i < cachedAsteroidTrack_.size(); ++i) {
        SDL_FPoint p1 = latLonToScreen(cachedAsteroidTrack_[i - 1].lat,
                                       cachedAsteroidTrack_[i - 1].lon);
        SDL_FPoint p2 = latLonToScreen(cachedAsteroidTrack_[i].lat,
                                       cachedAsteroidTrack_[i].lon);
        // Distance from mx,my to line segment p1-p2
        float l2 =
            (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y);
        if (l2 < 1.0f)
          continue;
        float t =
            ((mx - p1.x) * (p2.x - p1.x) + (my - p1.y) * (p2.y - p1.y)) / l2;
        t = std::max(0.0f, std::min(1.0f, t));
        float dist = std::sqrt((mx - (p1.x + t * (p2.x - p1.x))) *
                                   (mx - (p1.x + t * (p2.x - p1.x))) +
                               (my - (p1.y + t * (p2.y - p1.y))) *
                                   (my - (p1.y + t * (p2.y - p1.y))));
        if (dist < 4.0f) {
          hit = true;
          break;
        }
      }
    }

    if (hit) {
      AsteroidData data = asteroidProvider_->getLatest();
      for (const auto &ast : data.asteroids) {
        if (ast.name == state_->selectedAsteroidName) {
          std::string name = ast.name;
          if (name.size() > 2 && name.front() == '(' && name.back() == ')')
            name = name.substr(1, name.size() - 2);
          char buf[256];
          std::snprintf(buf, sizeof(buf), "%s\n%.2f LD  %.1f km/s%s",
                        name.c_str(), ast.missDistanceLD, ast.velocityKmS,
                        ast.isHazardous ? "\n[!] Potentially Hazardous" : "");
          tip = buf;
          break;
        }
      }
    }
  }

  // 7. Check NCDXF beacons
  if (tip.empty() && config_.showBeacons && beacons_) {
    // Only show if NCDXF widget is enabled in any pane's rotation (matching renderBeacons)
    bool widgetEnabled = false;
    for (auto *pane : panes_) {
      if (pane) {
        const auto &rotation = pane->getRotation();
        if (std::find(rotation.begin(), rotation.end(), std::string("ncdxf")) !=
            rotation.end()) {
          widgetEnabled = true;
          break;
        }
      }
    }
    if (widgetEnabled) {
      auto active = beacons_->getActiveBeacons();
      for (size_t i = 0; i < NCDXF_BEACONS.size(); ++i) {
        const auto &b = NCDXF_BEACONS[i];
        if (screenDist(b.lat, b.lon) < kHitRadius) {
          tip = "Beacon: " + b.callsign + "\n" + b.location;
          for (const auto &ab : active) {
            if (ab.index == (int)i) {
              tip += "\nTransmitting: " +
                     std::string(kBands[ab.bandIndex + 5].name);
              break;
            }
          }
          break;
        }
      }
    }
  }

  // 8. Check DX Cluster selected spot only (mirrors renderDXClusterSpots logic)
  if (tip.empty() && dxcStore_) {
    auto data = dxcStore_->snapshot();
    if (data->hasSelection && data->selectedSpot.txLat != 0.0) {
      const auto &spot = data->selectedSpot;
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
      }
    }
  }

  // 9. Check Live Spots (generic ones on map)
  if (tip.empty() && spotStore_) {
    auto data = spotStore_->snapshot();
    bool ofDe = config_.liveSpotsOfDe;
    bool useCall = config_.liveSpotsUseCall;
    std::string myLabel = useCall ? config_.callsign : config_.grid;
    if (myLabel.empty()) myLabel = "DE";

    for (const auto &spot : data->spots) {
      double rLat, rLon;
      if (Astronomy::gridToLatLon(spot.receiverGrid, rLat, rLon)) {
        if (screenDist(rLat, rLon) < kHitRadius) {
          if (ofDe) {
            // I (or my grid) spot THEM
            tip = spot.senderCallsign + " de " + myLabel;
          } else {
            // THEY spot ME (or my grid)
            tip = myLabel + " de " + spot.senderCallsign;
          }

          int bi = freqToBandIndex(spot.freqKhz);
          char buf[64];
          std::snprintf(buf, sizeof(buf), "\n%.1f kHz", spot.freqKhz);
          tip += buf;
          if (bi >= 0)
            tip += std::string(" (") + kBands[bi].name + ")";

          // Add Country name
          int row = std::clamp((int)((rLat + 90.0) * 2.0), 0, 359);
          int col = std::clamp((int)((rLon + 180.0) * 2.0), 0, 719);
          uint16_t cId = COUNTRY_GRID[row][col];
          if (cId > 0 && cId < NUM_COUNTRIES) {
            tip += "\n" + std::string(COUNTRY_NAMES[cId]);
          }

          break;
        }
      }
    }
  }

  // 10. Fallback to Country lookup
  if (tip.empty()) {
    int row = std::clamp((int)((lat + 90.0) * 2.0), 0, 359);
    int col = std::clamp((int)((lon + 180.0) * 2.0), 0, 719);
    uint16_t cId = COUNTRY_GRID[row][col];
    if (cId > 0 && cId < NUM_COUNTRIES) {
      tip = COUNTRY_NAMES[cId];
    }
  }

  if (tip.empty()) {
    tooltip_.visible = false;
    return;
  }


  // Trim trailing whitespace (common in TLE names)
  size_t last = tip.find_last_not_of(" \r\n\t");
  if (last != std::string::npos) {
    tip = tip.substr(0, last + 1);
  }

  tooltip_.text = tip;
  tooltip_.x = mx;
  tooltip_.y = my;
  tooltip_.visible = true;
  tooltip_.timestamp = SDL_GetTicks();
}

