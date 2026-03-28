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

// File-scope constants shared with MapWidget.cpp (redefined here; both are TU-local)
static constexpr const char *NIGHT_MAP_KEY = "night_map";
static constexpr const char *SAT_ICON_KEY = "sat_icon";
static constexpr const char *LINE_AA_KEY = "line_aa";

// Forward declaration for file-scope helper defined in MapWidget.cpp
void projectAzimuthal(double lat, double lon, double lat0, double lon0,
                      double &nx, double &ny);

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
    ThemeColors themes = getThemeColors(theme_);
    greatCircleVerts_.clear();
    greatCircleIndices_.clear();

    const auto &path = cachedGreatCircle_;
    float thickness = 1.2f;
    float r = thickness / 2.0f;
    SDL_Color color = themes.accent;  // Yellow -> Theme Accent

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
          double dLon = lon1_adj - lon0;
          double f = 0.5;
          if (std::fabs(dLon) > 1e-6) {
            f = (borderLon - lon0) / dLon;
          }
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
  // For azimuthal, we use higher density for smoother circular edges.
  const bool isAz = (config_.projection == "azimuthal" ||
                     config_.projection == "dual_azimuthal");
  const int gridW =
      useCompatibilityRenderPath_ ? (isAz ? 96 : 48) : (isAz ? 192 : 96);
  const int gridH =
      useCompatibilityRenderPath_ ? (isAz ? 48 : 24) : (isAz ? 96 : 48);

  // Constants matching original HamClock: 12 deg grayline, 0.75 power curve
  constexpr float GRAYLINE_COS = -0.21f;  // ~cos(90+12)
  constexpr float GRAYLINE_POW = 0.8f;  // Slightly steeper for deeper night

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
          float nf = (1.0f - fd) * 0.50f;  // Mute night shading to 50% to allow
                                           // overlays to show through

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

        // Check for texture wrapping (crossing date line) in Azimuthal
        bool wrap = false;
        if (config_.projection == "azimuthal" ||
            config_.projection == "dual_azimuthal") {
          float u0 = lightVerts_[p0].tex_coord.x;
          float u1 = lightVerts_[p1].tex_coord.x;
          float u2 = lightVerts_[p2].tex_coord.x;
          float u3 = lightVerts_[p3].tex_coord.x;
          if (std::abs(u0 - u1) > 0.5f || std::abs(u0 - u2) > 0.5f ||
              std::abs(u1 - u3) > 0.5f) {
            wrap = true;
          }
        }

        if (!wrap) {
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
        float darkness = (1.0f - fd) * 0.50f;
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

          float darkness = (1.0f - fd) * 0.50f;
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
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  ThemeColors themes = getThemeColors(theme_);

  for (int i = 0; i <= kSegments; ++i) {
    double theta = 2.0 * M_PI * i / kSegments;
    double pLat = lat + angRadDeg * std::cos(theta);
    double pLon = lon + angRadDeg * std::sin(theta) / cosLat;
    while (pLon > 180.0) pLon -= 360.0;
    while (pLon < -180.0) pLon += 360.0;

    if (i > 0) {
      double prevLon =
          lon + angRadDeg * std::sin(2.0 * M_PI * (i - 1) / kSegments) / cosLat;
      while (prevLon > 180.0) prevLon -= 360.0;
      while (prevLon < -180.0) prevLon += 360.0;

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
          RenderUtils::drawPolylineTextured(
              renderer, lineTex, segment.data(),
              static_cast<int>(segment.size()), 2.0f,
              {themes.accent.r, themes.accent.g, themes.accent.b, 120});
        }
        segment.clear();
        segment.push_back(latLonToScreen(borderLat, -borderLon));
      }
    }

    segment.push_back(latLonToScreen(pLat, pLon));
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(
        renderer, lineTex, segment.data(), static_cast<int>(segment.size()),
        2.0f, {themes.accent.r, themes.accent.g, themes.accent.b, 120});
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

  ThemeColors themes = getThemeColors(theme_);

  if (satTrackDirty_) {
    satTrackVerts_.clear();
    satTrackIndices_.clear();

    float thickness = 1.5f;
    float r = thickness / 2.0f;
    SDL_Color color = {themes.accent.r, themes.accent.g, themes.accent.b, 150};

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
          double dLon = lon1_adj - lon0;
          double f = (std::fabs(dLon) > 1e-6) ? (borderLon - lon0) / dLon : 0.5;
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

void MapWidget::onAsteroidElementsReady(const std::string &des,
                                        const OrbitalElements & /*elem*/) {
  if (state_ && des == state_->selectedAsteroidName) {
    asteroidTrackDirty_ = true;
    asteroidTrackVerts_.clear();
    asteroidTrackIndices_.clear();
  }
}

void MapWidget::renderAsteroidOverlay(SDL_Renderer *renderer) {
  if (!state_ || state_->selectedAsteroidName.empty())
    return;

  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  if (!lineTex)
    return;

  if (asteroidTrackDirty_ || cachedAsteroidTrack_.size() < 2) {
    // Rebuild geometry when dirty (track may be empty if elements not yet
    // arrived)
    asteroidTrackVerts_.clear();
    asteroidTrackIndices_.clear();

    if (cachedAsteroidTrack_.size() >= 2) {
      float thickness = 1.5f;
      float r = thickness / 2.0f;
      SDL_Color color = {config_.asteroidColor.r, config_.asteroidColor.g,
                         config_.asteroidColor.b, 200};

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
          int base = static_cast<int>(asteroidTrackVerts_.size());
          asteroidTrackVerts_.push_back(
              {{p1.x + nx, p1.y + ny}, color, {0, 0}});
          asteroidTrackVerts_.push_back(
              {{p1.x - nx, p1.y - ny}, color, {0, 1}});
          asteroidTrackVerts_.push_back(
              {{p2.x + nx, p2.y + ny}, color, {1, 0}});
          asteroidTrackVerts_.push_back(
              {{p2.x - nx, p2.y - ny}, color, {1, 1}});
          asteroidTrackIndices_.push_back(base + 0);
          asteroidTrackIndices_.push_back(base + 1);
          asteroidTrackIndices_.push_back(base + 2);
          asteroidTrackIndices_.push_back(base + 1);
          asteroidTrackIndices_.push_back(base + 2);
          asteroidTrackIndices_.push_back(base + 3);
        }
      };

      for (size_t i = 0; i < cachedAsteroidTrack_.size(); ++i) {
        if (i > 0) {
          double lon0 = cachedAsteroidTrack_[i - 1].lon;
          double lon1 = cachedAsteroidTrack_[i].lon;
          if (std::fabs(lon0 - lon1) > 180.0) {
            double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
            double borderLon = (lon1 < 0) ? 180.0 : -180.0;
            double dLon = lon1_adj - lon0;
            double f =
                (std::fabs(dLon) > 1e-6) ? (borderLon - lon0) / dLon : 0.5;
            double borderLat = cachedAsteroidTrack_[i - 1].lat +
                               f * (cachedAsteroidTrack_[i].lat -
                                    cachedAsteroidTrack_[i - 1].lat);
            segment.push_back(latLonToScreen(borderLat, borderLon));
            add_segment_geom(segment);
            segment.clear();
            segment.push_back(latLonToScreen(borderLat, -borderLon));
          }
        }
        segment.push_back(latLonToScreen(cachedAsteroidTrack_[i].lat,
                                         cachedAsteroidTrack_[i].lon));
      }
      if (segment.size() >= 2) {
        add_segment_geom(segment);
      }
      asteroidTrackDirty_ = false;
    }
  }

  SDL_RenderSetClipRect(renderer, &mapRect_);

  if (!asteroidTrackVerts_.empty()) {
    SDL_RenderGeometry(renderer, lineTex, asteroidTrackVerts_.data(),
                       (int)asteroidTrackVerts_.size(),
                       asteroidTrackIndices_.data(),
                       (int)asteroidTrackIndices_.size());
  }

  // Draw icon glyph at closest-approach point (center of track)
  if (!cachedAsteroidTrack_.empty()) {
    size_t mid = cachedAsteroidTrack_.size() / 2;
    SDL_FPoint sp = latLonToScreen(cachedAsteroidTrack_[mid].lat,
                                   cachedAsteroidTrack_[mid].lon);
    const std::string &icon =
        config_.asteroidIcon.empty() ? "☄" : config_.asteroidIcon;
    int ptSize = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
    int iw = 0, ih = 0;
    SDL_Color icnColor = {config_.asteroidColor.r, config_.asteroidColor.g,
                          config_.asteroidColor.b, 220};
    SDL_Texture *iconTex =
        fontMgr_.renderText(renderer, icon, icnColor, ptSize, &iw, &ih);
    if (iconTex) {
      SDL_Rect dst = {static_cast<int>(sp.x) - iw / 2,
                      static_cast<int>(sp.y) - ih / 2, iw, ih};
      SDL_RenderCopy(renderer, iconTex, nullptr, &dst);
      SDL_DestroyTexture(iconTex);
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSpotOverlay(SDL_Renderer *renderer) {
  if (!spotStore_)
    return;

  // Only show if Live Spots widget is enabled in any pane's rotation
  bool widgetEnabled = false;
  for (auto *pane : panes_) {
    if (pane) {
      const auto &rotation = pane->getRotation();
      if (std::find(rotation.begin(), rotation.end(), WidgetType::LIVE_SPOTS) !=
          rotation.end()) {
        widgetEnabled = true;
        break;
      }
    }
  }
  if (!widgetEnabled)
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
        double dLon = lon1_adj - lon0;
        double f = (std::fabs(dLon) > 1e-6) ? (borderLon - lon0) / dLon : 0.5;
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
    SDL_Color color = {255, 255, 255, 255};  // Default white
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
            double dLon = lon1_adj - lon0;
            double f =
                (std::fabs(dLon) > 1e-6) ? (borderLon - lon0) / dLon : 0.5;
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

    // Always show spot label next to selected spot (no hover required)
    SDL_FPoint sp = latLonToScreen(spot.txLat, spot.txLon);
    char labelBuf[64];
    int bi = freqToBandIndex(spot.freqKhz);
    if (bi >= 0)
      std::snprintf(labelBuf, sizeof(labelBuf), "%s %.1f %s",
                    spot.txCall.c_str(), spot.freqKhz, kBands[bi].name);
    else
      std::snprintf(labelBuf, sizeof(labelBuf), "%s %.1f kHz",
                    spot.txCall.c_str(), spot.freqKhz);
    fontMgr_.catalog()->drawText(renderer, labelBuf, static_cast<int>(sp.x) + 8,
                                 static_cast<int>(sp.y), color,
                                 FontStyle::Tiny);
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
    SDL_Color color = {255, 255, 255, 255};  // Default white
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

  // Sync with active filter
  if (config_.ontaFilter != "all") {
    if (StringUtils::toLower(spot.program) != config_.ontaFilter) {
      return;
    }
  }

  // Use coords from selectedSpot; if 0,0 (e.g. POTA parks CSV not yet loaded),
  // try to find updated coords in the live ontaSpots list.
  double spotLat = spot.lat, spotLon = spot.lon;
  if (spotLat == 0.0 && spotLon == 0.0) {
    for (const auto &s : data.ontaSpots) {
      if (s.call == spot.call && s.ref == spot.ref &&
          (s.lat != 0.0 || s.lon != 0.0)) {
        spotLat = s.lat;
        spotLon = s.lon;
        break;
      }
    }
  }
  if (spotLat == 0.0 && spotLon == 0.0)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);

  // Case-insensitive program check for color
  std::string lowerProg = StringUtils::toLower(spot.program);
  ThemeColors themes =
      getThemeColors(theme_);  // Added to access the current theme
  SDL_Color color = (lowerProg == "pota") ? themes.success : themes.info;

  LatLon de = state_->deLocation;
  auto path = Astronomy::calculateGreatCirclePath(de, {spotLat, spotLon}, 100);

  std::vector<SDL_FPoint> segment;
  SDL_Color lineColor = {color.r, color.g, color.b, 100};

  for (size_t i = 0; i < path.size(); ++i) {
    if (i > 0) {
      double lon0 = path[i - 1].lon;
      double lon1 = path[i].lon;
      if (std::fabs(lon0 - lon1) > 180.0) {
        double lon1_adj = (lon1 < 0) ? lon1 + 360.0 : lon1 - 360.0;
        double borderLon = (lon1 < 0) ? 180.0 : -180.0;
        double dLon = lon1_adj - lon0;
        double f = (std::fabs(dLon) > 1e-6) ? (borderLon - lon0) / dLon : 0.5;
        double borderLat =
            path[i - 1].lat + f * (path[i].lat - path[i - 1].lat);

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
  renderMarker(renderer, spotLat, spotLon, color.r, color.g, color.b,
               MarkerShape::Square, true);

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderBeacons(SDL_Renderer *renderer) {
  if (!config_.showBeacons || !beacons_)
    return;

  // Only show if NCDXF widget is enabled in any pane's rotation
  bool widgetEnabled = false;
  for (auto *pane : panes_) {
    if (pane) {
      const auto &rotation = pane->getRotation();
      if (std::find(rotation.begin(), rotation.end(), WidgetType::NCDXF) !=
          rotation.end()) {
        widgetEnabled = true;
        break;
      }
    }
  }
  if (!widgetEnabled)
    return;

  auto active = beacons_->getActiveBeacons();

  SDL_RenderSetClipRect(renderer, &mapRect_);

  for (size_t i = 0; i < NCDXF_BEACONS.size(); ++i) {
    const auto &b = NCDXF_BEACONS[i];

    // Check if this beacon is in the active list
    bool isTransmitting = false;
    for (const auto &ab : active) {
      if (ab.index == (int)i) {
        isTransmitting = true;
        break;
      }
    }

    if (isTransmitting) {
      // Bright Yellow for transmitting
      renderMarker(renderer, b.lat, b.lon, 255, 255, 0, MarkerShape::Circle,
                   true);
    } else {
      // Dim Gray for idle
      renderMarker(renderer, b.lat, b.lon, 100, 100, 100, MarkerShape::Circle,
                   true);
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderMufRtOverlay(SDL_Renderer *renderer) {
  if (config_.propOverlay != PropOverlayType::Muf)
    return;

  // MUF is now handled by renderPropagationOverlay using internal grid
}

void MapWidget::renderPropagationOverlay(SDL_Renderer *renderer) {
  if (config_.propOverlay == PropOverlayType::None ||
      config_.propOverlay == PropOverlayType::Aurora)
    return;

  if (!propTexture_)
    return;

  SDL_SetTextureAlphaMod(propTexture_, (Uint8)(config_.mufRtOpacity * 2.55f));
  SDL_SetTextureBlendMode(propTexture_, SDL_BLENDMODE_BLEND);

  SDL_RenderSetClipRect(renderer, &mapRect_);

  // Geometry buffer management
  const bool isAz = (config_.projection == "azimuthal" ||
                     config_.projection == "dual_azimuthal");
  const int gridW =
      useCompatibilityRenderPath_ ? (isAz ? 96 : 48) : (isAz ? 192 : 96);
  const int gridH =
      useCompatibilityRenderPath_ ? (isAz ? 48 : 24) : (isAz ? 96 : 48);

  // Ensure vertices are populated BEFORE built indices if projection or size
  // changed. This prevents 'wrap' detection from using uninitialized UV
  // coordinates.
  static std::string lastPropProj = "";
  static SDL_Rect lastMapRect = {0, 0, 0, 0};
  bool projChanged = (lastPropProj != config_.projection);
  bool rectChanged =
      (lastMapRect.x != mapRect_.x || lastMapRect.y != mapRect_.y ||
       lastMapRect.w != mapRect_.w || lastMapRect.h != mapRect_.h);

  if (projChanged || rectChanged ||
      propVerts_.size() != (size_t)((gridW + 1) * (gridH + 1))) {
    propVerts_.resize((gridW + 1) * (gridH + 1));
    if (config_.projection == "dual_azimuthal") {
      // Screen-space build — same approach as main map mesh for dual azimuthal
      for (int j = 0; j <= gridH; ++j) {
        float sy = mapRect_.y + (float)j * mapRect_.h / gridH;
        for (int i = 0; i <= gridW; ++i) {
          float sx = mapRect_.x + (float)i * mapRect_.w / gridW;
          int idx = j * (gridW + 1) + i;
          double lat, lon;
          if (screenToLatLon((int)sx, (int)sy, lat, lon)) {
            float u = static_cast<float>((lon + 180.0) / 360.0);
            float v = static_cast<float>((90.0 - lat) / 180.0);
            propVerts_[idx].position = {sx, sy};
            propVerts_[idx].color = {255, 255, 255, 190};
            propVerts_[idx].tex_coord = {u, v};
          } else {
            propVerts_[idx].position = {sx, sy};
            propVerts_[idx].color = {0, 0, 0, 0};
            propVerts_[idx].tex_coord = {0, 0};
          }
        }
      }
    } else {
      for (int j = 0; j <= gridH; ++j) {
        for (int i = 0; i <= gridW; ++i) {
          double lat = 90.0 - (double)j * 180.0 / gridH;
          double lon = -180.0 + (double)i * 360.0 / gridW;
          SDL_FPoint pt = latLonToScreen(lat, lon);
          int idx = j * (gridW + 1) + i;
          propVerts_[idx].position = {pt.x, pt.y};
          propVerts_[idx].color = {255, 255, 255, 190};
          propVerts_[idx].tex_coord = {(float)i / gridW, (float)j / gridH};
        }
      }
    }
    lastPropProj = config_.projection;
    lastMapRect = mapRect_;
  }

  // Now rebuild indices if the count is wrong or projection changed
  if (propIndices_.empty() || projChanged || rectChanged) {
    propIndices_.clear();
    propIndices_.reserve(gridW * gridH * 6);
    for (int j = 0; j < gridH; ++j) {
      for (int i = 0; i < gridW; ++i) {
        int p0 = j * (gridW + 1) + i;
        int p1 = p0 + 1;
        int p2 = (j + 1) * (gridW + 1) + i;
        int p3 = p2 + 1;

        // Check for texture wrapping (crossing date line)
        bool wrap = false;
        if (config_.projection == "azimuthal" ||
            config_.projection == "dual_azimuthal") {
          float u0 = propVerts_[p0].tex_coord.x;
          float u1 = propVerts_[p1].tex_coord.x;
          float u2 = propVerts_[p2].tex_coord.x;
          float u3 = propVerts_[p3].tex_coord.x;
          if (std::abs(u0 - u1) > 0.5f || std::abs(u0 - u2) > 0.5f ||
              std::abs(u1 - u3) > 0.5f) {
            wrap = true;
          }
        }

        if (!wrap) {
          propIndices_.push_back(p0);
          propIndices_.push_back(p1);
          propIndices_.push_back(p2);
          propIndices_.push_back(p2);
          propIndices_.push_back(p1);
          propIndices_.push_back(p3);
        }
      }
    }
  }

  if (config_.projection == "robinson" || config_.projection == "azimuthal" ||
      config_.projection == "dual_azimuthal") {
    SDL_RenderGeometry(renderer, propTexture_, propVerts_.data(),
                       (int)propVerts_.size(), propIndices_.data(),
                       (int)propIndices_.size());
  } else {
    SDL_RenderCopy(renderer, propTexture_, nullptr, &mapRect_);
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderGribCloudOverlay(SDL_Renderer *renderer) {
  if (config_.weatherOverlay != WeatherOverlayType::CloudsGrib)
    return;
  if (!gribCloud_)
    return;

  // Pick up any newly decoded surface and upload to GPU.
  SDL_Surface *surf = gribCloud_->takeSurface();
  if (surf) {
    if (gribCloudFillTex_)
      MemoryMonitor::getInstance().destroyTexture(gribCloudFillTex_);
    gribCloudFillTex_ = SDL_CreateTextureFromSurface(renderer, surf);
    if (gribCloudFillTex_) {
      MemoryMonitor::getInstance().addVram((int64_t)surf->w * surf->h * 4);
      SDL_SetTextureBlendMode(gribCloudFillTex_, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(surf);
  }

  if (!gribCloudFillTex_)
    return;

  // Use alpha 140 (approx 55%) for a good balance of "whiteness" and
  // transparency. Note: We use vertex alpha because some drivers ignore
  // SDL_SetTextureAlphaMod when using SDL_RenderGeometry (mesh-based
  // rendering).
  uint8_t targetAlpha = 140;

  SDL_RenderSetClipRect(renderer, &mapRect_);

  if (config_.projection != "equirectangular" && !mapVerts_.empty()) {
    // Manually modulate vertex colors/alpha.
    for (auto &v : mapVerts_) {
      v.color.r = 255;
      v.color.g = 255;
      v.color.b = 255;
      v.color.a = targetAlpha;
    }

    SDL_RenderGeometry(renderer, gribCloudFillTex_, mapVerts_.data(),
                       (int)mapVerts_.size(), nightIndices_.data(),
                       (int)nightIndices_.size());

    // RESET vertex colors for subsequent overlays (MapWidget reuses mapVerts_)
    for (auto &v : mapVerts_) {
      v.color = {255, 255, 255, 255};
    }
  } else {
    SDL_SetTextureColorMod(gribCloudFillTex_, 255, 255, 255);
    SDL_SetTextureAlphaMod(gribCloudFillTex_, targetAlpha);
    SDL_RenderCopy(renderer, gribCloudFillTex_, nullptr, &mapRect_);
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderWxMbOverlay(SDL_Renderer *renderer) {
  if (config_.weatherOverlay != WeatherOverlayType::WxMb)
    return;
  if (!wxmb_)
    return;

  // Poll for new GFS data — rebuild GPU buffers when segments arrive.
  {
    std::vector<WxSegment> segs;
    std::vector<WxQuiver> quivers;
    SDL_Surface *fillSurface = nullptr;
    if (wxmb_->getSegments(segs, quivers, fillSurface)) {
      wxVerts_.clear();
      wxIndices_.clear();

      if (fillSurface) {
        if (wxFillTex_) {
          MemoryMonitor::getInstance().destroyTexture(wxFillTex_);
          wxFillTex_ = nullptr;
        }
        wxFillTex_ = SDL_CreateTextureFromSurface(renderer, fillSurface);
        if (wxFillTex_) {
          MemoryMonitor::getInstance().addVram((int64_t)fillSurface->w *
                                               fillSurface->h * 4);
          SDL_SetTextureBlendMode(wxFillTex_, SDL_BLENDMODE_BLEND);
          SDL_SetTextureAlphaMod(wxFillTex_, 200);  // 80% opacity fill
        }
        SDL_FreeSurface(fillSurface);
      }

      SDL_Color mainColor = {220, 240, 255, 200};  // cool blue-white isobar
      SDL_Color glowColor = {0, 30, 60, 120};  // subtle midnight-blue glow

      int numPasses = (config_.projection == "dual_azimuthal") ? 2 : 1;
      double wxDeLat = state_ ? state_->deLocation.lat : 0.0;
      double wxDeLon = state_ ? state_->deLocation.lon : 0.0;
      double wxAntiLat = -wxDeLat;
      double wxAntiLon = wxDeLon + (wxDeLon >= 0.0 ? -180.0 : 180.0);
      int wxHalfW = mapRect_.w / 2;
      float wxHemisR = std::min(wxHalfW, mapRect_.h) * 0.5f;

      auto wxProjectPt = [&](double lat, double lon, int pass) -> SDL_FPoint {
        if (config_.projection == "dual_azimuthal") {
          double cLat = (pass == 0) ? wxDeLat : wxAntiLat;
          double cLon = (pass == 0) ? wxDeLon : wxAntiLon;
          float cx = (pass == 0) ? (mapRect_.x + wxHalfW * 0.5f)
                                 : (mapRect_.x + wxHalfW + wxHalfW * 0.5f);
          float cy = mapRect_.y + mapRect_.h * 0.5f;
          double nx, ny;
          projectAzimuthal(lat, lon, cLat, cLon, nx, ny);
          return {cx + (float)nx * wxHemisR, cy - (float)ny * wxHemisR};
        }
        return latLonToScreen(lat, lon);
      };

      for (int pass = 0; pass < numPasses; ++pass) {
        float jumpThresh = (config_.projection == "dual_azimuthal")
                               ? wxHemisR * 2.0f
                               : (float)mapRect_.w * 0.5f;

        // --- Isobar contour segments -----------------------------------------
        for (const auto &seg : segs) {
          SDL_FPoint p1 = wxProjectPt(seg.lat1, seg.lon1, pass);
          SDL_FPoint p2 = wxProjectPt(seg.lat2, seg.lon2, pass);

          // Skip segments that jump across the antimeridian seam or hemisphere
          // edge.
          if (std::abs(p1.x - p2.x) > jumpThresh)
            continue;

          float dx = p2.x - p1.x;
          float dy = p2.y - p1.y;
          float len = std::sqrt(dx * dx + dy * dy);
          if (len < 0.1f)
            continue;
          dx /= len;
          dy /= len;

          // Layer 1: Dark glow (2.0f thick)
          {
            float t = 2.0f * 0.5f;
            float nx = -dy * t, ny = dx * t;
            int s = (int)wxVerts_.size();
            wxVerts_.push_back({{p1.x + nx, p1.y + ny}, glowColor, {0, 0}});
            wxVerts_.push_back({{p1.x - nx, p1.y - ny}, glowColor, {0, 1}});
            wxVerts_.push_back({{p2.x + nx, p2.y + ny}, glowColor, {0, 0}});
            wxVerts_.push_back({{p2.x - nx, p2.y - ny}, glowColor, {0, 1}});
            wxIndices_.insert(wxIndices_.end(),
                              {s, s + 1, s + 2, s + 2, s + 1, s + 3});
          }
          // Layer 2: Main isobar line (1.2f thick)
          {
            float t = 1.2f * 0.5f;
            float nx = -dy * t, ny = dx * t;
            int s = (int)wxVerts_.size();
            wxVerts_.push_back({{p1.x + nx, p1.y + ny}, mainColor, {0, 0}});
            wxVerts_.push_back({{p1.x - nx, p1.y - ny}, mainColor, {0, 1}});
            wxVerts_.push_back({{p2.x + nx, p2.y + ny}, mainColor, {0, 0}});
            wxVerts_.push_back({{p2.x - nx, p2.y - ny}, mainColor, {0, 1}});
            wxIndices_.insert(wxIndices_.end(),
                              {s, s + 1, s + 2, s + 2, s + 1, s + 3});
          }
        }

        // --- Wind quiver arrows
        // -----------------------------------------------
        SDL_Color arrowColor = {180, 220, 255, 160};
        for (const auto &q : quivers) {
          float speed = std::sqrt(q.u * q.u + q.v * q.v);
          if (speed < 0.5f)
            continue;

          SDL_FPoint origin = wxProjectPt(q.lat, q.lon, pass);

          float arrowLen = std::clamp(speed * 1.5f, 4.0f, 20.0f);
          float angle =
              std::atan2(-(q.v), q.u);  // negate v because y increases down
          SDL_FPoint tip = {origin.x + std::cos(angle) * arrowLen,
                            origin.y + std::sin(angle) * arrowLen};

          if (std::abs(origin.x - tip.x) > jumpThresh)
            continue;

          float shaftdx = tip.x - origin.x;
          float shaftdy = tip.y - origin.y;
          float slen = std::sqrt(shaftdx * shaftdx + shaftdy * shaftdy);
          if (slen < 0.5f)
            continue;
          float sdx = shaftdx / slen, sdy = shaftdy / slen;

          // Shaft (1px)
          {
            float t = 0.6f;
            float nx = -sdy * t, ny = sdx * t;
            int s = (int)wxVerts_.size();
            wxVerts_.push_back(
                {{origin.x + nx, origin.y + ny}, arrowColor, {0, 0}});
            wxVerts_.push_back(
                {{origin.x - nx, origin.y - ny}, arrowColor, {0, 1}});
            wxVerts_.push_back({{tip.x + nx, tip.y + ny}, arrowColor, {0, 0}});
            wxVerts_.push_back({{tip.x - nx, tip.y - ny}, arrowColor, {0, 1}});
            wxIndices_.insert(wxIndices_.end(),
                              {s, s + 1, s + 2, s + 2, s + 1, s + 3});
          }
          // Arrowhead barbs
          float headLen = std::max(3.0f, arrowLen * 0.4f);
          for (int sign : {-1, 1}) {
            float ha = angle + 3.14159f + sign * 0.45f;
            SDL_FPoint barb = {tip.x + std::cos(ha) * headLen,
                               tip.y + std::sin(ha) * headLen};
            float bdx = barb.x - tip.x, bdy = barb.y - tip.y;
            float bl = std::sqrt(bdx * bdx + bdy * bdy);
            if (bl < 0.1f)
              continue;
            bdx /= bl;
            bdy /= bl;
            float t = 0.6f;
            float nx = -bdy * t, ny = bdx * t;
            int s = (int)wxVerts_.size();
            wxVerts_.push_back({{tip.x + nx, tip.y + ny}, arrowColor, {0, 0}});
            wxVerts_.push_back({{tip.x - nx, tip.y - ny}, arrowColor, {0, 1}});
            wxVerts_.push_back(
                {{barb.x + nx, barb.y + ny}, arrowColor, {0, 0}});
            wxVerts_.push_back(
                {{barb.x - nx, barb.y - ny}, arrowColor, {0, 1}});
            wxIndices_.insert(wxIndices_.end(),
                              {s, s + 1, s + 2, s + 2, s + 1, s + 3});
          }
        }
      }  // end pass loop
    }
  }

  SDL_RenderSetClipRect(renderer, &mapRect_);

  // Render pressure fill layer underneath
  if (wxFillTex_ && config_.propOverlay == PropOverlayType::None) {
    if (config_.projection != "equirectangular" && !mapVerts_.empty()) {
      SDL_RenderGeometry(renderer, wxFillTex_, mapVerts_.data(),
                         (int)mapVerts_.size(), nightIndices_.data(),
                         (int)nightIndices_.size());
    } else {
      SDL_RenderCopy(renderer, wxFillTex_, nullptr, &mapRect_);
    }
  }

  // Render AA isobar lines and quivers on top
  if (!wxVerts_.empty()) {
    SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
    SDL_RenderGeometry(renderer, lineTex, wxVerts_.data(), (int)wxVerts_.size(),
                       wxIndices_.data(), (int)wxIndices_.size());
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}
void MapWidget::renderLegend(SDL_Renderer *renderer) {
  if (config_.propOverlay == PropOverlayType::None)
    return;

  // Legend position: bottom-right of map area
  int legendW = 120;
  int legendH = 12;
  int pad = 6;
  int lx = x_ + width_ - legendW - pad - 18;  // shifted left for text overhang
  int ly = y_ + height_ - legendH - pad - 22;  // Above RSS button if active

  // Labels and Scale
  std::string labelMin, labelMax, title;
  PropOverlayType type = config_.propOverlay;

  if (type == PropOverlayType::Muf) {
    title = "MUF (MHz)";
    labelMin = "0";
    labelMax = "50";
  } else if (type == PropOverlayType::Reliability) {
    title = "Rel (%)";
    labelMin = "0";
    labelMax = "100";
  } else if (type == PropOverlayType::Toa) {
    title = "TOA (deg)";
    labelMin = "0";
    labelMax = "40";
  } else if (type == PropOverlayType::Drap) {
    title = "Abs (MHz)";
    labelMin = "0";
    labelMax = "30+";
  } else if (type == PropOverlayType::Heatmap) {
    title = "Reach";
    labelMin = "Low";
    labelMax = "High";
  } else if (type == PropOverlayType::Aurora) {
    title = "Aurora (%)";
    labelMin = "0";
    labelMax = "100";
  } else {
    // VOACAP uses Reliability scale/colors internally
    labelMin = "0";
    labelMax = "100";
  }

  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_);
  SDL_Color txtCol = themes.text;

  // Draw Title
  cat->drawText(renderer, title, lx + legendW / 2, ly - 10, txtCol,
                FontStyle::Micro, true, false, true);

  // Draw Legend Strip (gradient)
  SDL_Rect strip = {lx, ly, legendW, legendH};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
  SDL_RenderFillRect(renderer, &strip);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &strip);

  // Draw 10 segments to approximate the color scale
  for (int i = 0; i < 10; ++i) {
    float t = (float)i / 9.0f;
    uint8_t r = 0, g = 0, b = 0;

    // Reuse color logic from onPropDataReady/drapColor
    if (type == PropOverlayType::Drap) {
      float mhz = t * 30.0f;
      if (mhz < 5.0f) {
        r = 255;
        g = 255;
        b = 0;
      } else if (mhz < 15.0f) {
        r = 255;
        g = 140;
        b = 0;
      } else {
        r = 220;
        g = 50;
        b = 50;
      }
    } else if (type == PropOverlayType::Reliability ||
               type == PropOverlayType::Voacap) {
      if (t < 0.5f) {
        float f = t / 0.5f;
        r = (uint8_t)(100 + f * 155);
        g = (uint8_t)(100 + f * 155);
        b = 100;
      } else {
        float f = (t - 0.5f) / 0.5f;
        r = (uint8_t)(255 * (1.0f - f));
        g = 255;
        b = (uint8_t)(100 * (1.0f - f));
      }
    } else if (type == PropOverlayType::Toa) {
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
    } else if (type == PropOverlayType::Heatmap) {
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
    } else if (type == PropOverlayType::Aurora) {
      // Aurora: black -> green
      r = 0;
      g = (uint8_t)(t * 255);
      b = 0;
    } else {  // MUF
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

    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_Rect seg = {lx + i * legendW / 10, ly + 1, legendW / 10 + 1,
                    legendH - 2};
    SDL_RenderFillRect(renderer, &seg);
  }

  // Draw Min/Max Labels
  cat->drawText(renderer, labelMin, lx, ly + legendH + 8, txtCol,
                FontStyle::Micro, false, false, true);
  cat->drawText(renderer, labelMax, lx + legendW, ly + legendH + 8, txtCol,
                FontStyle::Micro, true, false, true);
}

void MapWidget::renderWxMbLegend(SDL_Renderer *renderer) {
  if (config_.weatherOverlay != WeatherOverlayType::WxMb || !wxFillTex_)
    return;

  // Do not render WX/Pressure legend if a propagation overlay is also active
  // to avoid color confusion with the map.
  if (config_.propOverlay != PropOverlayType::None)
    return;

  int legendW = 120;
  int legendH = 12;
  int pad = 6;
  int lx = x_ + width_ - legendW - pad - 18;  // shifted left for text overhang
  int ly = y_ + height_ - legendH - pad - 22;  // Above RSS button if active

  // If a propagation overlay legend is already rendered, move this one up
  if (config_.propOverlay != PropOverlayType::None) {
    ly -= (legendH + 28);
  }

  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_);
  SDL_Color txtCol = themes.text;

  // Draw Title
  cat->drawText(renderer, "Pressure (hPa)", lx + legendW / 2, ly - 10, txtCol,
                FontStyle::Micro, true, false, true);

  // Draw Legend Strip (gradient)
  SDL_Rect strip = {lx, ly, legendW, legendH};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
  SDL_RenderFillRect(renderer, &strip);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &strip);

  // Color stops match buildSegments:
  struct Stop {
    float hPa;
    uint8_t r, g, b;
  };
  static const Stop kStops[] = {
      {960.0f, 20, 40, 200},     {990.0f, 100, 160, 240},
      {1013.25f, 240, 240, 248}, {1025.0f, 255, 160, 110},
      {1040.0f, 195, 25, 25},
  };
  static constexpr int kNStops = 5;

  auto pressureToColor = [&](float hpa) -> SDL_Color {
    if (hpa <= kStops[0].hPa)
      return {kStops[0].r, kStops[0].g, kStops[0].b, 255};
    if (hpa >= kStops[kNStops - 1].hPa)
      return {kStops[kNStops - 1].r, kStops[kNStops - 1].g,
              kStops[kNStops - 1].b, 255};
    for (int i = 0; i < kNStops - 1; ++i) {
      if (hpa <= kStops[i + 1].hPa) {
        float t = (hpa - kStops[i].hPa) / (kStops[i + 1].hPa - kStops[i].hPa);
        auto lerp8 = [&](uint8_t a, uint8_t b) -> uint8_t {
          return (uint8_t)(a + (b - a) * t);
        };
        return {lerp8(kStops[i].r, kStops[i + 1].r),
                lerp8(kStops[i].g, kStops[i + 1].g),
                lerp8(kStops[i].b, kStops[i + 1].b), 255};
      }
    }
    return {240, 240, 248, 255};
  };

  // Draw 20 segments to approximate the color scale
  float minHpa = 960.0f;
  float maxHpa = 1040.0f;
  int numSegs = 20;
  for (int i = 0; i < numSegs; ++i) {
    float t = (float)i / (float)(numSegs - 1);
    float hpa = minHpa + t * (maxHpa - minHpa);
    SDL_Color c = pressureToColor(hpa);

    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_Rect seg = {lx + i * legendW / numSegs, ly + 1, legendW / numSegs + 1,
                    legendH - 2};
    SDL_RenderFillRect(renderer, &seg);
  }

  // Draw Min/Max Labels
  cat->drawText(renderer, "960", lx, ly + legendH + 8, txtCol, FontStyle::Micro,
                false, false, true);
  cat->drawText(renderer, "1040", lx + legendW, ly + legendH + 8, txtCol,
                 FontStyle::Micro, true, false, true);
}
 
void MapWidget::renderCloudLegend(SDL_Renderer *renderer) {
  if (config_.weatherOverlay != WeatherOverlayType::CloudsGrib || !gribCloudFillTex_)
    return;

  int legendW = 120; // Compact standard width
  int legendH = 12;
  int pad = 6;
  int lx = x_ + width_ - legendW - pad - 18;
  int ly = y_ + height_ - legendH - pad - 22;

  // Stack above Prop and Wx legends
  if (config_.propOverlay != PropOverlayType::None) {
    ly -= (legendH + 28);
  }
  if (config_.weatherOverlay == WeatherOverlayType::WxMb) {
    ly -= (legendH + 28);
  }

  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_);
  SDL_Color txtCol = themes.text;

  // Draw Background Box for entire legend area (title + bar + labels)
  SDL_Rect bgRect = {lx - 10, ly - 22, legendW + 20, legendH + 42};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
  SDL_RenderFillRect(renderer, &bgRect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bgRect);

  // Draw Title
  cat->drawText(renderer, "Cloud Height", lx + legendW / 2, ly - 10, txtCol,
                FontStyle::Micro, true, false, true);

  // Draw Legend Strip (grayscale gradient)
  SDL_Rect strip = {lx, ly, legendW, legendH};
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &strip);

  // Approximate grayscale gradient
  int numSegs = 40;
  for (int i = 0; i < numSegs; ++i) {
    float t = (float)i / (float)(numSegs - 1);
    uint8_t alpha = (uint8_t)(140.0f * std::pow(t, 1.5f));
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);
    SDL_Rect seg = {lx + i * legendW / numSegs, ly + 1, legendW / numSegs + 1,
                    legendH - 2};
    SDL_RenderFillRect(renderer, &seg);
  }

  // Draw Labels evenly spaced for better balance
  cat->drawText(renderer, "Low", lx + legendW * 1 / 6, ly + legendH + 8, txtCol, FontStyle::Micro,
                true, false, true);
  cat->drawText(renderer, "Mid", lx + legendW * 3 / 6, ly + legendH + 8, txtCol, FontStyle::Micro,
                true, false, true);
  cat->drawText(renderer, "High", lx + legendW * 5 / 6, ly + legendH + 8, txtCol, FontStyle::Micro,
                true, false, true);
}

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

  int ptSize = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
  int tw = fontMgr_.getLogicalWidth(tooltip_.text, ptSize);
  int th = fontMgr_.getLogicalHeight(tooltip_.text, ptSize);

  // Only create new texture if text changed
  if (tooltip_.text != tooltip_.cachedText || !tooltip_.cachedTexture) {
    // Clean up old texture
    MemoryMonitor::getInstance().destroyTexture(tooltip_.cachedTexture);

    // Create new texture
    int actualW = 0, actualH = 0;
    tooltip_.cachedTexture = fontMgr_.renderText(renderer, tooltip_.text,
                                                 getThemeColors(theme_).text,
                                                 ptSize, &actualW, &actualH);

    if (!tooltip_.cachedTexture) {
      LOG_E("MapWidget", "Failed to create tooltip texture: {}",
            SDL_GetError());
      return;
    }

    tooltip_.cachedText = tooltip_.text;
    tooltip_.cachedW = actualW;
    tooltip_.cachedH = actualH;
    tw = actualW;
    th = actualH;
  } else {
    // Reuse cached texture
    tw = tooltip_.cachedW;
    th = tooltip_.cachedH;
  }

  int padX = 8, padY = 5;
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
    by = tooltip_.y + 16;  // flip below cursor

  // Background
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 210);
  SDL_Rect bg = {bx, by, boxW, boxH};
  SDL_RenderFillRect(renderer, &bg);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 200);
  SDL_RenderDrawRect(renderer, &bg);

  // Text (using cached texture)
  SDL_Rect dst = {bx + padX, by + padY, tw, th};
  SDL_RenderCopy(renderer, tooltip_.cachedTexture, nullptr, &dst);
}

void MapWidget::renderGridOverlay(SDL_Renderer *renderer) {
  if (!config_.showGrid)
    return;

  SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
  if (!lineTex)
    return;

  SDL_Color color = {100, 100, 100, 128};
  float thickness = 1.0f;

  auto draw_safe_polyline = [&](const std::vector<LatLon> &path) {
    std::vector<SDL_FPoint> segment;
    for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0) {
        // Jump detection: if longitude jumps > 180 or distance is huge
        double dlon = std::abs(path[i].lon - path[i - 1].lon);
        bool jump = (dlon > 180.0);

        if (!jump && (config_.projection == "azimuthal" ||
                      config_.projection == "dual_azimuthal")) {
          // Also check for huge screen jumps (e.g. crossing the disk edge)
          SDL_FPoint p1 = latLonToScreen(path[i - 1].lat, path[i - 1].lon);
          SDL_FPoint p2 = latLonToScreen(path[i].lat, path[i].lon);
          float dx = p2.x - p1.x;
          float dy = p2.y - p1.y;
          float jumpThresh = (config_.projection == "dual_azimuthal")
                                 ? std::min(mapRect_.w / 2, mapRect_.h) * 0.5f
                                 : std::min(mapRect_.w, mapRect_.h) * 0.5f;
          if (std::sqrt(dx * dx + dy * dy) > jumpThresh) {
            jump = true;
          }
        }

        if (jump) {
          if (segment.size() >= 2) {
            RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                              (int)segment.size(), thickness,
                                              color, false);
          }
          segment.clear();
        }
      }
      segment.push_back(latLonToScreen(path[i].lat, path[i].lon));
    }
    if (segment.size() >= 2) {
      RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                        (int)segment.size(), thickness, color,
                                        false);
    }
  };

  if (config_.gridType == "latlon") {
    // Draw latitude lines every 15 degrees
    for (int lat = -75; lat <= 75; lat += 15) {
      std::vector<LatLon> path;
      for (int lon = -180; lon <= 180; lon += 5) {
        path.push_back({(double)lat, (double)lon});
      }
      draw_safe_polyline(path);
    }

    // Draw longitude lines every 30 degrees
    for (int lon = -180; lon < 180; lon += 30) {
      std::vector<LatLon> path;
      for (int lat = -85; lat <= 85; lat += 5) {
        path.push_back({(double)lat, (double)lon});
      }
      draw_safe_polyline(path);
    }
  } else if (config_.gridType == "maidenhead") {
    // Draw Maidenhead field lines
    for (int field_lon = 0; field_lon <= 18; ++field_lon) {
      double lon = -180.0 + field_lon * 20.0;
      std::vector<LatLon> path;
      for (int lat = -85; lat <= 85; lat += 5) {
        path.push_back({(double)lat, lon});
      }
      draw_safe_polyline(path);
    }

    for (int field_lat = 0; field_lat <= 18; ++field_lat) {
      double lat = -90.0 + field_lat * 10.0;
      if (lat < -85 || lat > 85)
        continue;
      std::vector<LatLon> path;
      for (int lon = -180; lon <= 180; lon += 5) {
        path.push_back({lat, (double)lon});
      }
      draw_safe_polyline(path);
    }
  }
}
void MapWidget::renderAuroraOverlay(SDL_Renderer *renderer) {
  if (!auroraMapStore_ || config_.propOverlay != PropOverlayType::Aurora)
    return;

  AuroraMapData data = auroraMapStore_->get();
  if (!data.valid)
    return;

  SDL_RenderSetClipRect(renderer, &mapRect_);

  bool needsUpdate = !auroraTexture_ ||
                     auroraVerts_.empty() ||
                     (lastAuroraProjection_ != config_.projection) ||
                     (data.lastUpdate > lastAuroraUpdateTime_);

  if (needsUpdate) {
    // Step 1: Build auroraTexture_ (360x181 RGBA) from aurora grid data.
    // texRow=0 → lat=90 (north), texRow=180 → lat=-90 (south).
    // texCol=0 → lon=-180, texCol=359 → lon=179.
    if (!auroraTexture_) {
      auroraTexture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STATIC, 360, 181);
      if (!auroraTexture_) {
        SDL_RenderSetClipRect(renderer, nullptr);
        return;
      }
      SDL_SetTextureBlendMode(auroraTexture_, SDL_BLENDMODE_BLEND);
    }

    std::vector<uint32_t> pixels(360 * 181);
    for (int texRow = 0; texRow <= 180; ++texRow) {
      int dataRow = 180 - texRow;
      for (int texCol = 0; texCol < 360; ++texCol) {
        int dataCol = (texCol + 180) % 360;
        uint8_t val = data.grid[dataRow * 360 + dataCol];
        // Exclude equatorial latitudes — the NOAA OVATION model produces
        // background noise (val 1–8) near lat=0 with a gap to lat=±46 where
        // the real aurora ovals begin.  |lat| < 20 is physically aurora-free
        // under all but the most extreme (G5+) conditions.
        int ilat = dataRow - 90; // geographic latitude of this texture row
        uint8_t alpha = 0;
        if (val >= 5 && std::abs(ilat) >= 20)
          alpha = static_cast<uint8_t>(std::clamp(40 + val * 160 / 100, 0, 200));
        // RGBA32 little-endian: R=byte0, G=byte1, B=byte2, A=byte3
        // Aurora is green: R=0, G=255, B=0
        pixels[texRow * 360 + texCol] =
            ((uint32_t)alpha << 24) | (0u << 16) | (255u << 8) | 0u;
      }
    }
    SDL_UpdateTexture(auroraTexture_, nullptr, pixels.data(),
                      360 * sizeof(uint32_t));

    // Step 2: Build UV-mapped mesh — same pattern as renderPropagationOverlay.
    const bool isAz = (config_.projection == "azimuthal" ||
                       config_.projection == "dual_azimuthal");
    const int gridW = isAz ? 192 : 96;
    const int gridH = isAz ? 96  : 48;

    auroraVerts_.clear();
    auroraIndices_.clear();
    auroraVerts_.resize((gridW + 1) * (gridH + 1));

    if (config_.projection == "dual_azimuthal") {
      // Screen-space sampling: iterate pixels, invert to lat/lon, compute UV.
      for (int j = 0; j <= gridH; ++j) {
        float sy = mapRect_.y + (float)j * mapRect_.h / gridH;
        for (int i = 0; i <= gridW; ++i) {
          float sx = mapRect_.x + (float)i * mapRect_.w / gridW;
          int idx = j * (gridW + 1) + i;
          double lat, lon;
          if (screenToLatLon((int)sx, (int)sy, lat, lon)) {
            float u = static_cast<float>((lon + 180.0) / 360.0);
            float v = static_cast<float>((90.0 - lat) / 180.0);
            auroraVerts_[idx].position = {sx, sy};
            auroraVerts_[idx].color = {255, 255, 255, 255};
            auroraVerts_[idx].tex_coord = {u, v};
          } else {
            auroraVerts_[idx].position = {sx, sy};
            auroraVerts_[idx].color = {0, 0, 0, 0};
            auroraVerts_[idx].tex_coord = {0, 0};
          }
        }
      }
    } else {
      for (int j = 0; j <= gridH; ++j) {
        for (int i = 0; i <= gridW; ++i) {
          double lat = 90.0 - (double)j * 180.0 / gridH;
          double lon = -180.0 + (double)i * 360.0 / gridW;
          SDL_FPoint pt = latLonToScreen(lat, lon);
          int idx = j * (gridW + 1) + i;
          auroraVerts_[idx].position = {pt.x, pt.y};
          auroraVerts_[idx].color = {255, 255, 255, 255};
          auroraVerts_[idx].tex_coord = {(float)i / gridW, (float)j / gridH};
        }
      }
    }

    // Step 3: Build index buffer with UV-based wrap detection.
    auroraIndices_.reserve(gridW * gridH * 6);
    for (int j = 0; j < gridH; ++j) {
      for (int i = 0; i < gridW; ++i) {
        int p0 = j * (gridW + 1) + i;
        int p1 = p0 + 1;
        int p2 = (j + 1) * (gridW + 1) + i;
        int p3 = p2 + 1;

        bool wrap = false;
        if (config_.projection == "azimuthal" ||
            config_.projection == "dual_azimuthal") {
          float u0 = auroraVerts_[p0].tex_coord.x;
          float u1 = auroraVerts_[p1].tex_coord.x;
          float u2 = auroraVerts_[p2].tex_coord.x;
          float u3 = auroraVerts_[p3].tex_coord.x;
          if (std::abs(u0 - u1) > 0.5f || std::abs(u0 - u2) > 0.5f ||
              std::abs(u1 - u3) > 0.5f)
            wrap = true;
        }

        if (!wrap) {
          auroraIndices_.push_back(p0);
          auroraIndices_.push_back(p1);
          auroraIndices_.push_back(p2);
          auroraIndices_.push_back(p2);
          auroraIndices_.push_back(p1);
          auroraIndices_.push_back(p3);
        }
      }
    }

    lastAuroraUpdateTime_ = data.lastUpdate;
    lastAuroraProjection_ = config_.projection;
  }

  if (auroraTexture_ && !auroraVerts_.empty()) {
    SDL_RenderGeometry(renderer, auroraTexture_, auroraVerts_.data(),
                       (int)auroraVerts_.size(), auroraIndices_.data(),
                       (int)auroraIndices_.size());
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderProjectionSelect(SDL_Renderer *renderer) {
  // Show "Map View ▼" to indicate it opens a menu
  std::string label = "Map View \xE2\x96\xBC";  // ▼ in UTF-8

  // Position at top-left of Widget (independent of centered mapRect_)
  projRect_ = {x_ + 4, y_ + 4, 100, 22};

  ThemeColors themes = getThemeColors(theme_);

  // Draw semi-transparent background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 180);
  SDL_RenderFillRect(renderer, &projRect_);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &projRect_);

  auto *cat = fontMgr_.catalog();
  // Text
  cat->drawText(renderer, label, projRect_.x + projRect_.w / 2,
                projRect_.y + projRect_.h / 2, themes.text, FontStyle::Micro,
                true, false, true);
}

void MapWidget::renderRssButton(SDL_Renderer *renderer) {
  // Draw "RSS" toggle button at top-right of the map area
  rssRect_ = {x_ + width_ - 48, y_ + 4, 44, 22};

  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 160);
  SDL_RenderFillRect(renderer, &rssRect_);

  SDL_Color col = config_.rssEnabled ? themes.success : themes.textDim;
  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  SDL_RenderDrawRect(renderer, &rssRect_);

  fontMgr_.catalog()->drawText(renderer, "RSS", rssRect_.x + rssRect_.w / 2,
                               rssRect_.y + rssRect_.h / 2, col,
                               FontStyle::Micro, true, false, true);
}

void MapWidget::renderOverlayInfo(SDL_Renderer *renderer) {
  if (config_.propOverlay == PropOverlayType::None &&
      config_.weatherOverlay == WeatherOverlayType::None)
    return;

  std::string text;
  if (config_.propOverlay == PropOverlayType::Muf) {
    text = "MUF Overlay (RT)";
  } else if (config_.propOverlay == PropOverlayType::Voacap) {
    text = fmt::format("VOACAP ({} / {} / {}W)", config_.propBand,
                       config_.propMode, config_.propPower);
  } else if (config_.propOverlay == PropOverlayType::Reliability) {
    text = fmt::format("Reliability ({} / {} / {}W)", config_.propBand,
                       config_.propMode, config_.propPower);
  } else if (config_.propOverlay == PropOverlayType::Toa) {
    text = "TOA Overlay";
  } else if (config_.propOverlay == PropOverlayType::Drap) {
    text = "DRAP Absorption";
  } else if (config_.propOverlay == PropOverlayType::Heatmap) {
    text = "Reach Heatmap";
  } else if (config_.propOverlay == PropOverlayType::Aurora) {
    text = "Aurora Forecast";
  }

  if (config_.weatherOverlay == WeatherOverlayType::WxMb) {
    if (!text.empty())
      text += " + ";
    text += "WX/Pressure";
  } else if (config_.weatherOverlay == WeatherOverlayType::CloudsGrib) {
    if (!text.empty())
      text += " + ";
    text += "Clouds (GFS)";
  }

  if (text.empty())
    return;

  int ptSize = fontMgr_.catalog()->ptSize(FontStyle::SmallRegular);
  int textW = fontMgr_.getLogicalWidth(text, ptSize, true);
  int textH = 20;  // Approx for 14pt (simplified)
  int padX = 12;
  int padY = 4;
  int boxW = textW + padX * 2;
  int boxH = textH + padY * 2;

  int cx = x_ + width_ / 2;
  int cy = y_ + 20;  // Top margin

  SDL_Rect box = {cx - boxW / 2, cy - boxH / 2, boxW, boxH};

  ThemeColors themes = getThemeColors(theme_);

  // Box
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         180);  // Dark semi-transparent
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);  // Border
  SDL_RenderDrawRect(renderer, &box);

  // Text
  fontMgr_.catalog()->drawText(renderer, text, cx, cy, themes.text,
                               FontStyle::Micro, true, false, true);
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

void MapWidget::renderAzimuthalMask(SDL_Renderer *renderer) {
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);

  if (config_.projection == "dual_azimuthal") {
    // Two side-by-side circles
    int halfW = mapRect_.w / 2;
    float R = std::min(halfW, mapRect_.h) * 0.5f - 6.0f;
    float cxL = mapRect_.x + halfW * 0.5f;
    float cxR = mapRect_.x + halfW + halfW * 0.5f;
    float cy = mapRect_.y + mapRect_.h * 0.5f;

    for (int j = 0; j < mapRect_.h; ++j) {
      float sy = mapRect_.y + j;
      float dy = std::abs(sy - cy);

      if (dy >= R) {
        // Entire row outside both circles
        SDL_Rect fullRow = {mapRect_.x, (int)sy, mapRect_.w, 1};
        SDL_RenderFillRect(renderer, &fullRow);
      } else {
        float dx = std::sqrt(R * R - dy * dy);

        // Left of left circle
        int xL0 = (int)(cxL - dx);
        SDL_Rect rL0 = {mapRect_.x, (int)sy, xL0 - mapRect_.x, 1};
        if (rL0.w > 0)
          SDL_RenderFillRect(renderer, &rL0);

        // Gap between circles
        int xL1 = (int)(cxL + dx);
        int xR0 = (int)(cxR - dx);
        if (xR0 > xL1) {
          SDL_Rect rGap = {xL1, (int)sy, xR0 - xL1, 1};
          SDL_RenderFillRect(renderer, &rGap);
        }

        // Right of right circle
        int xR1 = (int)(cxR + dx);
        int mapRight = mapRect_.x + mapRect_.w;
        SDL_Rect rR1 = {xR1, (int)sy, mapRight - xR1, 1};
        if (rR1.w > 0)
          SDL_RenderFillRect(renderer, &rR1);
      }
    }
    return;
  }

  // Single azimuthal circle
  float cx = mapRect_.x + mapRect_.w * 0.5f;
  float cy = mapRect_.y + mapRect_.h * 0.5f;
  float radius = std::min(mapRect_.w, mapRect_.h) * 0.5f - 5.0f;

  for (int j = 0; j < mapRect_.h; ++j) {
    float sy = mapRect_.y + j;
    float dy = std::abs(sy - cy);

    if (dy >= radius) {
      // Entire row is outside the circle vertically
      SDL_Rect fullRow = {mapRect_.x, (int)sy, mapRect_.w, 1};
      SDL_RenderFillRect(renderer, &fullRow);
    } else {
      float dx = std::sqrt(radius * radius - dy * dy);

      // Left masking: from mapRect_.x up to cx - dx
      int xMaskEnd = (int)(cx - dx);
      SDL_Rect leftCrop = {mapRect_.x, (int)sy, xMaskEnd - mapRect_.x, 1};
      if (leftCrop.w > 0)
        SDL_RenderFillRect(renderer, &leftCrop);

      // Right masking: from cx + dx to mapRect_.x + mapRect_.w
      int xMaskStart = (int)(cx + dx);
      SDL_Rect rightCrop = {xMaskStart, (int)sy,
                            (mapRect_.x + mapRect_.w) - xMaskStart, 1};
      if (rightCrop.w > 0)
        SDL_RenderFillRect(renderer, &rightCrop);
    }
  }
}

void MapWidget::renderCountryBorders(SDL_Renderer *renderer) {
  if (borderDirty_) {
    borderVerts_.clear();
    borderIndices_.clear();

    SDL_Color mainColor = {200, 200, 200, 180};  // Crisp light grey
    SDL_Color glowColor = {0, 0, 0, 100};  // Subtle dark glow/shadow

    // For dual_azimuthal: build geometry for both hemispheres
    int numPasses = (config_.projection == "dual_azimuthal") ? 2 : 1;
    double deLat = state_ ? state_->deLocation.lat : 0.0;
    double deLon = state_ ? state_->deLocation.lon : 0.0;
    double antiLat = -deLat;
    double antiLon = deLon + (deLon >= 0.0 ? -180.0 : 180.0);
    int halfW = mapRect_.w / 2;
    float hemisR = std::min(halfW, mapRect_.h) * 0.5f;

    for (int pass = 0; pass < numPasses; ++pass) {
      // Project a lat/lon to screen for this pass
      auto projectPt = [&](double lat, double lon) -> SDL_FPoint {
        if (config_.projection == "dual_azimuthal") {
          double cLat = (pass == 0) ? deLat : antiLat;
          double cLon = (pass == 0) ? deLon : antiLon;
          float cx = (pass == 0) ? (mapRect_.x + halfW * 0.5f)
                                 : (mapRect_.x + halfW + halfW * 0.5f);
          float cy = mapRect_.y + mapRect_.h * 0.5f;
          double nx, ny;
          projectAzimuthal(lat, lon, cLat, cLon, nx, ny);
          return {cx + (float)nx * hemisR, cy - (float)ny * hemisR};
        }
        return latLonToScreen(lat, lon);
      };
      float jumpThresh = (config_.projection == "dual_azimuthal")
                             ? hemisR * 2.0f
                             : mapRect_.w * 0.5f;

      for (uint32_t i = 0; i < g_NumWorldBorders; ++i) {
        const auto &line = g_WorldBorders[i];
        if (line.numPts < 2)
          continue;

        std::vector<SDL_FPoint> pts;
        pts.reserve(line.numPts);
        for (uint16_t j = 0; j < line.numPts; ++j) {
          double lat = line.pts[j].lat / 100.0;
          double lon = line.pts[j].lon / 100.0;
          pts.push_back(projectPt(lat, lon));
        }

        for (size_t j = 0; j < pts.size() - 1; ++j) {
          if (std::abs(pts[j].x - pts[j + 1].x) > jumpThresh)
            continue;

          float x1 = pts[j].x, y1 = pts[j].y;
          float x2 = pts[j + 1].x, y2 = pts[j + 1].y;
          float dx = x2 - x1;
          float dy = y2 - y1;
          float len = std::sqrt(dx * dx + dy * dy);
          if (len < 0.1f)
            continue;
          dx /= len;
          dy /= len;

          // Layer 1: Subtle Dark Glow (Thicker)
          {
            float thickness = 2.2f;
            float nx = -dy * (thickness * 0.5f);
            float ny = dx * (thickness * 0.5f);
            int startIdx = (int)borderVerts_.size();

            borderVerts_.push_back({{x1 + nx, y1 + ny}, glowColor, {0, 0}});
            borderVerts_.push_back({{x1 - nx, y1 - ny}, glowColor, {0, 1}});
            borderVerts_.push_back({{x2 + nx, y2 + ny}, glowColor, {0, 0}});
            borderVerts_.push_back({{x2 - nx, y2 - ny}, glowColor, {0, 1}});

            borderIndices_.push_back(startIdx + 0);
            borderIndices_.push_back(startIdx + 1);
            borderIndices_.push_back(startIdx + 2);
            borderIndices_.push_back(startIdx + 2);
            borderIndices_.push_back(startIdx + 1);
            borderIndices_.push_back(startIdx + 3);
          }

          // Layer 2: Main Border Line (Thinner)
          {
            float thickness = 1.2f;
            float nx = -dy * (thickness * 0.5f);
            float ny = dx * (thickness * 0.5f);
            int startIdx = (int)borderVerts_.size();

            borderVerts_.push_back({{x1 + nx, y1 + ny}, mainColor, {0, 0}});
            borderVerts_.push_back({{x1 - nx, y1 - ny}, mainColor, {0, 1}});
            borderVerts_.push_back({{x2 + nx, y2 + ny}, mainColor, {0, 0}});
            borderVerts_.push_back({{x2 - nx, y2 - ny}, mainColor, {0, 1}});

            borderIndices_.push_back(startIdx + 0);
            borderIndices_.push_back(startIdx + 1);
            borderIndices_.push_back(startIdx + 2);
            borderIndices_.push_back(startIdx + 2);
            borderIndices_.push_back(startIdx + 1);
            borderIndices_.push_back(startIdx + 3);
          }
        }
      }
    }  // end pass loop
    borderDirty_ = false;
    LOG_D("MapWidget", "Country borders geometry cached ({} vertices)",
          (int)borderVerts_.size());
  }

  if (!borderVerts_.empty()) {
    SDL_Texture *lineTex = texMgr_.get(LINE_AA_KEY);
    SDL_RenderGeometry(renderer, lineTex, borderVerts_.data(),
                       (int)borderVerts_.size(), borderIndices_.data(),
                       (int)borderIndices_.size());
  }
}

// ---------------------------------------------------------------------------
// Calendar alert overlay
// ---------------------------------------------------------------------------

void MapWidget::showCalendarAlert(const std::string &summary,
                                  const std::string &source,
                                  time_t startTime, int dismissMinutes) {
  calendarAlert_.active = true;
  calendarAlert_.summary = summary;
  calendarAlert_.source = source;
  calendarAlert_.startTime = startTime;
  calendarAlert_.shownAtMs = SDL_GetTicks();
  calendarAlert_.durationMs = (uint32_t)(dismissMinutes * 60 * 1000);
}

void MapWidget::renderCalendarAlert(SDL_Renderer *renderer) {
  if (!calendarAlert_.active)
    return;

  uint32_t elapsed = SDL_GetTicks() - calendarAlert_.shownAtMs;
  if (elapsed >= calendarAlert_.durationMs) {
    calendarAlert_.active = false;
    return;
  }

  if (!fontMgr_.ready())
    return;
  auto *cat = fontMgr_.catalog();

  // Panel dimensions: 60% of map width, centered
  const int panW = (int)(mapRect_.w * 0.60);
  const int panH = 110;
  const int panX = mapRect_.x + (mapRect_.w - panW) / 2;
  const int panY = mapRect_.y + (mapRect_.h - panH) / 2;

  // Background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 10, 10, 30, 220);
  SDL_Rect panRect = {panX, panY, panW, panH};
  SDL_RenderFillRect(renderer, &panRect);

  // Border
  SDL_SetRenderDrawColor(renderer, 100, 160, 255, 255);
  SDL_RenderDrawRect(renderer, &panRect);

  // Header: "CALENDAR ALERT"
  ThemeColors themes = getThemeColors(theme_);
  cat->drawText(renderer, "CALENDAR ALERT", panX + panW / 2, panY + 8,
                themes.accent, FontStyle::MicroBold, true);

  // Event summary
  std::string summary = calendarAlert_.summary;
  if (summary.size() > 48)
    summary = summary.substr(0, 47) + "\xe2\x80\xa6";
  cat->drawText(renderer, summary.c_str(), panX + panW / 2, panY + 30,
                themes.text, FontStyle::UIBold, true);

  // Source + start time
  char timeBuf[24];
  struct tm t{};
  Astronomy::portable_gmtime(&calendarAlert_.startTime, &t);
  std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02dZ", t.tm_hour, t.tm_min);

  std::string sub = calendarAlert_.source + "  \xe2\x80\xa2  " + timeBuf;
  cat->drawText(renderer, sub.c_str(), panX + panW / 2, panY + 54,
                themes.textDim, FontStyle::Caption, true);

  // Progress bar (counts down to 0)
  float frac = 1.0f - (float)elapsed / (float)calendarAlert_.durationMs;
  int barW = (int)((panW - 16) * frac);
  SDL_SetRenderDrawColor(renderer, 100, 160, 255, 180);
  SDL_Rect bar = {panX + 8, panY + panH - 12, barW, 6};
  SDL_RenderFillRect(renderer, &bar);

  // Dismiss hint
  cat->drawText(renderer, "tap to dismiss", panX + panW / 2, panY + panH - 20,
                themes.textDim, FontStyle::Micro, true);
}

// ---------------------------------------------------------------------------
// Star field — real star positions from Yale BSC subset, rendered in the
// non-projection area for Azimuthal and Robinson projections.
// Stars are positioned using an equirectangular celestial mapping that rotates
// with Greenwich Mean Sidereal Time (GMST), giving a slowly moving sky.
// Only stars outside the Earth projection mask are drawn.
// ---------------------------------------------------------------------------
void MapWidget::renderStarField(SDL_Renderer *renderer) {
  const std::string &proj = config_.projection;
  if (proj != "azimuthal" && proj != "robinson")
    return;

  // GMST in degrees: determines which RA is currently at the Greenwich meridian.
  double GMST_deg = Astronomy::calculateGST(std::chrono::system_clock::now()) * 15.0;

  // For azimuthal: precompute circle exclusion test parameters.
  float az_cx = 0, az_cy = 0, az_R2 = 0;
  if (proj == "azimuthal") {
    az_cx = mapRect_.x + mapRect_.w * 0.5f;
    az_cy = mapRect_.y + mapRect_.h * 0.5f;
    float R = std::min(mapRect_.w, mapRect_.h) * 0.5f;
    az_R2 = R * R;
  }

  // Synthetic background stars — generated once, cached.
  // 600 stars uniformly distributed on the celestial sphere (LCG seeded).
  // These are unnamed (no tooltip) and fill in coverage where the named catalog
  // is sparse, especially in Robinson's narrow corner areas.
  struct BgStar { float ra_deg, dec_deg, mag; };
  static std::vector<BgStar> s_bgStars;
  if (s_bgStars.empty()) {
    s_bgStars.reserve(600);
    uint32_t lcg = 0xDEADBEEFu;
    auto next = [&]() -> uint32_t {
      lcg = lcg * 1664525u + 1013904223u;
      return lcg;
    };
    for (int i = 0; i < 600; ++i) {
      float ra = (next() & 0xFFFF) * (360.0f / 65535.0f);
      float v  = (next() & 0xFFFF) / 65535.0f * 2.0f - 1.0f;  // −1..1 → uniform sphere
      float dec = std::asin(v) * (180.0f / static_cast<float>(M_PI));
      float mag = 4.0f + (next() & 0xFF) * (2.0f / 255.0f);  // 4.0–6.0
      s_bgStars.push_back({ra, dec, mag});
    }
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Helper: render one star (named or background). Returns without drawing if
  // the star falls inside the visible map projection area.
  auto drawStar = [&](float ra_deg, float dec_deg, float mag) {
    double ra_frac = std::fmod((ra_deg - GMST_deg) / 360.0 + 10.0, 1.0);
    int sx = x_ + static_cast<int>(ra_frac * width_);
    int sy = y_ + static_cast<int>((90.0f - dec_deg) / 180.0f * height_);

    if (sx < x_ || sx >= x_ + width_ || sy < y_ || sy >= y_ + height_)
      return;

    bool inMap = false;
    if (proj == "azimuthal") {
      float dx = sx - az_cx, dy = sy - az_cy;
      inMap = (dx * dx + dy * dy) <= az_R2;
    } else {
      // Robinson: use round-trip to detect visible-oval vs black corner.
      double lat, lon;
      if (!screenToLatLon(sx, sy, lat, lon)) {
        inMap = false;
      } else {
        SDL_FPoint fwd = latLonToScreen(lat, lon);
        inMap = (std::abs(fwd.x - sx) <= 2.0f && std::abs(fwd.y - sy) <= 2.0f);
      }
    }
    if (inMap)
      return;

    // Magnitude-based brightness: brightest (-1.5) → 255, faintest (6.0) → 55.
    float brightness = std::max(0.0f, std::min(1.0f, (6.0f - mag) / 7.5f));
    auto lum = static_cast<uint8_t>(55 + static_cast<int>(brightness * 200));

    SDL_SetRenderDrawColor(renderer, lum, lum, lum, 255);
    SDL_RenderDrawPoint(renderer, sx, sy);

    if (mag < 1.5f) {
      SDL_RenderDrawPoint(renderer, sx - 1, sy);
      SDL_RenderDrawPoint(renderer, sx + 1, sy);
      SDL_RenderDrawPoint(renderer, sx, sy - 1);
      SDL_RenderDrawPoint(renderer, sx, sy + 1);
    } else if (mag < 2.5f) {
      SDL_RenderDrawPoint(renderer, sx + 1, sy);
      SDL_RenderDrawPoint(renderer, sx, sy + 1);
    }
  };

  for (std::size_t i = 0; i < kBrightStarsCount; ++i)
    drawStar(kBrightStars[i].ra_deg, kBrightStars[i].dec_deg, kBrightStars[i].mag);

  for (const BgStar &bg : s_bgStars)
    drawStar(bg.ra_deg, bg.dec_deg, bg.mag);
}
