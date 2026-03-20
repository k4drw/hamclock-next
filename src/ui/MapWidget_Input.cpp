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

  // 7. Check DX Cluster selected spot only (mirrors renderDXClusterSpots logic)
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

