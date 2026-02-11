#include "MapWidget.h"
#include "../core/Astronomy.h"
#include "../core/LiveSpotData.h"
#include "RenderUtils.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>

static constexpr const char *MAP_KEY = "earth_map";
static constexpr const char *MAP_PATH = "assets/map_background.png";
static constexpr const char *SAT_ICON_KEY = "sat_icon";
static constexpr const char *SAT_ICON_PATH = "assets/satellite.png";
static constexpr int FALLBACK_W = 1024;
static constexpr int FALLBACK_H = 512;

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

SDL_Point MapWidget::latLonToScreen(double lat, double lon) const {
  double nx = (lon + 180.0) / 360.0;
  double ny = (90.0 - lat) / 180.0;
  int px = mapRect_.x + static_cast<int>(nx * mapRect_.w);
  int py = mapRect_.y + static_cast<int>(ny * mapRect_.h);
  return {px, py};
}

void MapWidget::update() {
  auto now = std::chrono::system_clock::now();
  auto sun = Astronomy::sunPosition(now);
  sunLat_ = sun.lat;
  sunLon_ = sun.lon;
}

bool MapWidget::onMouseUp(int mx, int my, Uint16 mod) {
  // Hit test against map rect
  if (mx < mapRect_.x || mx >= mapRect_.x + mapRect_.w || my < mapRect_.y ||
      my >= mapRect_.y + mapRect_.h) {
    return false;
  }

  // Inverse projection: screen -> lat/lon
  double nx = static_cast<double>(mx - mapRect_.x) / mapRect_.w;
  double ny = static_cast<double>(my - mapRect_.y) / mapRect_.h;
  double lon = nx * 360.0 - 180.0;
  double lat = 90.0 - ny * 180.0;

  if (mod & KMOD_SHIFT) {
    // Shift-click: set DE (current location)
    state_->deLocation = {lat, lon};
    state_->deGrid = Astronomy::latLonToGrid(lat, lon);
    // Note: Actual persistent config update happens in SetupScreen or via a
    // dedicated service, but this updates the live state immediately.
  } else {
    // Normal click: set DX (target)
    state_->dxLocation = {lat, lon};
    state_->dxGrid = Astronomy::latLonToGrid(lat, lon);
    state_->dxActive = true;
  }

  return true;
}

void MapWidget::renderMarker(SDL_Renderer *renderer, double lat, double lon,
                             Uint8 r, Uint8 g, Uint8 b, MarkerShape shape,
                             bool outline) {
  SDL_Point pt = latLonToScreen(lat, lon);
  float radius = 3.0f; // Default small radius

  // Context-aware radius: if it's the large Sun/DE/DX marker
  if (shape == MarkerShape::Circle && r == 255 && g == 255 && b == 0) {
    // Sun
    radius = std::max(4.0f, std::min(mapRect_.w, mapRect_.h) / 60.0f);
  } else if (shape == MarkerShape::Circle) {
    // DE/DX or Spot Transmitter
    radius = std::max(3.0f, std::min(mapRect_.w, mapRect_.h) / 80.0f);
  } else {
    // Spot Receiver (Square)
    radius = 2.0f;
  }

  if (shape == MarkerShape::Circle) {
    RenderUtils::drawCircle(renderer, static_cast<float>(pt.x),
                            static_cast<float>(pt.y), radius, {r, g, b, 255});
    if (outline) {
      RenderUtils::drawCircleOutline(renderer, static_cast<float>(pt.x),
                                     static_cast<float>(pt.y), radius,
                                     {0, 0, 0, 255});
    }
  } else {
    RenderUtils::drawRect(renderer, pt.x - radius, pt.y - radius, radius * 2,
                          radius * 2, {r, g, b, 255});
    if (outline) {
      RenderUtils::drawRectOutline(renderer, pt.x - radius, pt.y - radius,
                                   radius * 2, radius * 2, {0, 0, 0, 255});
    }
  }
}

void MapWidget::renderGreatCircle(SDL_Renderer *renderer) {
  if (!state_->dxActive)
    return;

  LatLon de = state_->deLocation;
  LatLon dx = state_->dxLocation;

  auto path = Astronomy::calculateGreatCirclePath(de, dx, 250);
  SDL_Color color = {255, 255, 0, 255}; // Yellow

  std::vector<SDL_FPoint> points;
  for (const auto &ll : path) {
    SDL_Point pt = latLonToScreen(ll.lat, ll.lon);
    points.push_back({static_cast<float>(pt.x), static_cast<float>(pt.y)});
  }

  // Handle dateline wrapping by drawing segments
  std::vector<SDL_FPoint> segment;
  for (size_t i = 0; i < points.size(); ++i) {
    if (i > 0) {
      // Check if this segment wraps
      float lon1 = path[i - 1].lon;
      float lon2 = path[i].lon;
      if (std::fabs(lon1 - lon2) > 180.0) {
        // Draw the current segment and start a new one
        if (segment.size() >= 2) {
          RenderUtils::drawPolyline(renderer, segment.data(),
                                    static_cast<int>(segment.size()), 2.0f,
                                    color);
        }
        segment.clear();
      }
    }
    segment.push_back(points[i]);
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolyline(renderer, segment.data(),
                              static_cast<int>(segment.size()), 2.0f, color);
  }
}

void MapWidget::renderNightOverlay(SDL_Renderer *renderer) {
  auto terminator = Astronomy::calculateTerminator(sunLat_, sunLon_);
  int N = static_cast<int>(terminator.size());
  if (N < 2)
    return;

  bool nightBelow = (sunLat_ >= 0);
  float edgeY = nightBelow ? static_cast<float>(mapRect_.y + mapRect_.h)
                           : static_cast<float>(mapRect_.y);

  SDL_Color nightColor = {0, 0, 0, 100};

  std::vector<SDL_Vertex> verts(2 * N);
  for (int i = 0; i < N; ++i) {
    SDL_Point tp = latLonToScreen(terminator[i].lat, terminator[i].lon);

    verts[2 * i].position = {static_cast<float>(tp.x),
                             static_cast<float>(tp.y)};
    verts[2 * i].color = nightColor;
    verts[2 * i].tex_coord = {0, 0};

    verts[2 * i + 1].position = {static_cast<float>(tp.x), edgeY};
    verts[2 * i + 1].color = nightColor;
    verts[2 * i + 1].tex_coord = {0, 0};
  }

  std::vector<int> indices;
  indices.reserve((N - 1) * 6);
  for (int i = 0; i < N - 1; ++i) {
    int tl = 2 * i;
    int bl = 2 * i + 1;
    int tr = 2 * (i + 1);
    int br = 2 * (i + 1) + 1;

    indices.push_back(tl);
    indices.push_back(bl);
    indices.push_back(tr);

    indices.push_back(tr);
    indices.push_back(bl);
    indices.push_back(br);
  }

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_RenderGeometry(renderer, nullptr, verts.data(),
                     static_cast<int>(verts.size()), indices.data(),
                     static_cast<int>(indices.size()));
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::render(SDL_Renderer *renderer) {
  // Black background for letterbox bars
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  // Load or generate map texture on first render
  if (!mapLoaded_) {
    SDL_Texture *tex = texMgr_.loadImage(renderer, MAP_KEY, MAP_PATH);
    if (!tex) {
      tex = texMgr_.generateEarthFallback(renderer, MAP_KEY, FALLBACK_W,
                                          FALLBACK_H);
    }
    mapLoaded_ = true;
  }

  // 1. Earth texture
  SDL_Texture *mapTex = texMgr_.get(MAP_KEY);
  if (mapTex) {
    SDL_RenderCopy(renderer, mapTex, nullptr, &mapRect_);
  }

  // 2. Night overlay
  renderNightOverlay(renderer);

  // 3. Great circle path (yellow line, DE to DX)
  renderGreatCircle(renderer);

  // 4. DE marker (orange)
  renderMarker(renderer, state_->deLocation.lat, state_->deLocation.lon, 255,
               165, 0);

  // 5. DX marker (green, if active)
  if (state_->dxActive) {
    renderMarker(renderer, state_->dxLocation.lat, state_->dxLocation.lon, 0,
                 255, 0);
  }

  // 6. Satellite overlays (footprint, ground track, icon)
  renderSatellite(renderer);

  // 7. Live spot overlays (markers + great circle paths for selected bands)
  renderSpotOverlay(renderer);

  // 8. Sun marker (yellow, with outline)
  renderMarker(renderer, sunLat_, sunLon_, 255, 255, 0, MarkerShape::Circle,
               true);

  // 9. Pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);
}

void MapWidget::renderSatellite(SDL_Renderer *renderer) {
  if (!predictor_ || !predictor_->isReady())
    return;

  SubSatPoint ssp = predictor_->subSatPoint();

  // 1. Footprint circle
  renderSatFootprint(renderer, ssp.lat, ssp.lon, ssp.footprint);

  // 2. Ground track (next 90 minutes)
  renderSatGroundTrack(renderer);

  // 3. Satellite icon at sub-satellite point
  SDL_Point pt = latLonToScreen(ssp.lat, ssp.lon);
  int iconSz = std::max(16, std::min(mapRect_.w, mapRect_.h) / 25);

  SDL_Texture *satTex = texMgr_.get(SAT_ICON_KEY);
  if (!satTex) {
    satTex = texMgr_.loadImage(renderer, SAT_ICON_KEY, SAT_ICON_PATH);
  }
  if (satTex) {
    SDL_Rect dst = {pt.x - iconSz / 2, pt.y - iconSz / 2, iconSz, iconSz};
    SDL_RenderCopy(renderer, satTex, nullptr, &dst);
  }
}

void MapWidget::renderSatFootprint(SDL_Renderer *renderer, double lat,
                                   double lon, double footprintKm) {
  if (footprintKm <= 0.0)
    return;

  // Angular radius in degrees (footprint is diameter)
  constexpr double kKmPerDeg = 111.32;
  double angRadDeg = (footprintKm / 2.0) / kKmPerDeg;

  double latRad = lat * 3.14159265358979323846 / 180.0;
  double cosLat = std::cos(latRad);
  if (std::fabs(cosLat) < 0.01)
    cosLat = 0.01; // avoid div by zero near poles

  // Draw circle as line segments
  constexpr int kSegments = 72;
  SDL_SetRenderDrawColor(renderer, 255, 255, 0, 120);

  SDL_RenderSetClipRect(renderer, &mapRect_);

  std::vector<SDL_FPoint> segment;
  SDL_Point prev{};
  for (int i = 0; i <= kSegments; ++i) {
    double theta = 2.0 * 3.14159265358979323846 * i / kSegments;
    double pLat = lat + angRadDeg * std::cos(theta);
    double pLon = lon + angRadDeg * std::sin(theta) / cosLat;

    // Normalize longitude
    while (pLon > 180.0)
      pLon -= 360.0;
    while (pLon < -180.0)
      pLon += 360.0;

    SDL_Point cur = latLonToScreen(pLat, pLon);
    if (i > 0) {
      // Check if this segment wraps
      if (std::abs(cur.x - prev.x) > mapRect_.w / 2) {
        if (segment.size() >= 2) {
          RenderUtils::drawPolyline(renderer, segment.data(),
                                    static_cast<int>(segment.size()), 1.0f,
                                    {255, 255, 0, 120});
        }
        segment.clear();
      }
    }
    segment.push_back({static_cast<float>(cur.x), static_cast<float>(cur.y)});
    prev = cur;
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolyline(renderer, segment.data(),
                              static_cast<int>(segment.size()), 1.0f,
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

  // Draw ground track as thin segments
  for (size_t i = 1; i < track.size(); ++i) {
    if (std::fabs(track[i].lon - track[i - 1].lon) > 180.0)
      continue;

    SDL_Point p1 = latLonToScreen(track[i - 1].lat, track[i - 1].lon);
    SDL_Point p2 = latLonToScreen(track[i].lat, track[i].lon);

    RenderUtils::drawThickLine(
        renderer, static_cast<float>(p1.x), static_cast<float>(p1.y),
        static_cast<float>(p2.x), static_cast<float>(p2.y), 1.0f,
        {255, 200, 0, 150});
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::renderSpotOverlay(SDL_Renderer *renderer) {
  if (!spotStore_)
    return;
  auto data = spotStore_->get();
  if (!data.valid || data.spots.empty())
    return;

  // Check if any bands are selected
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

  for (const auto &spot : data.spots) {
    int bandIdx = freqToBandIndex(spot.freqKhz);
    if (bandIdx < 0 || !data.selectedBands[bandIdx])
      continue;

    double lat, lon;
    if (!Astronomy::gridToLatLon(spot.receiverGrid, lat, lon))
      continue;

    const auto &bc = kBands[bandIdx].color;

    // Great circle path from DE to receiver
    auto path = Astronomy::calculateGreatCirclePath(de, {lat, lon}, 100);
    SDL_Color color = {bc.r, bc.g, bc.b, 180};

    std::vector<SDL_FPoint> segment;
    for (size_t i = 0; i < path.size(); ++i) {
      if (i > 0 && std::fabs(path[i].lon - path[i - 1].lon) > 180.0) {
        if (segment.size() >= 2) {
          RenderUtils::drawPolyline(renderer, segment.data(),
                                    static_cast<int>(segment.size()), 1.0f,
                                    color);
        }
        segment.clear();
      }
      SDL_Point pt = latLonToScreen(path[i].lat, path[i].lon);
      segment.push_back({static_cast<float>(pt.x), static_cast<float>(pt.y)});
    }
    if (segment.size() >= 2) {
      RenderUtils::drawPolyline(renderer, segment.data(),
                                static_cast<int>(segment.size()), 1.0f, color);
    }

    // Marker at receiver location (Square for receiver)
    renderMarker(renderer, lat, lon, bc.r, bc.g, bc.b, MarkerShape::Square,
                 true);
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcMapRect();
}
