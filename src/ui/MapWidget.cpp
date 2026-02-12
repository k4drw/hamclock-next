#include "MapWidget.h"
#include "../core/Astronomy.h"
#include "../core/LiveSpotData.h"
#include "EmbeddedIcons.h"
#include "RenderUtils.h"

#include <algorithm>

#include <chrono>
#include <cmath>
#include <vector>

static constexpr const char *MAP_KEY = "earth_map";
static constexpr const char *SAT_ICON_KEY = "sat_icon";
static constexpr const char *LINE_AA_KEY = "line_aa";
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

SDL_FPoint MapWidget::latLonToScreen(double lat, double lon) const {
  double nx = (lon + 180.0) / 360.0;
  double ny = (90.0 - lat) / 180.0;
  float px = static_cast<float>(mapRect_.x + nx * mapRect_.w);
  float py = static_cast<float>(mapRect_.y + ny * mapRect_.h);
  return {px, py};
}

static const char *kMonthNames[] = {
    "january", "february", "march",     "april",   "may",      "june",
    "july",    "august",   "september", "october", "november", "december"};

void MapWidget::update() {
  auto now = std::chrono::system_clock::now();
  auto sun = Astronomy::sunPosition(now);
  sunLat_ = sun.lat;
  sunLon_ = sun.lon;

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

    std::fprintf(stderr, "MapWidget: starting async fetch for %s\n", url);
    netMgr_.fetchAsync(
        url,
        [this, url_str = std::string(url)](std::string data) {
          if (!data.empty()) {
            std::fprintf(stderr, "MapWidget: received %zu bytes for %s\n",
                         data.size(),
                         url_str.size() > 50 ? "NASA Map" : url_str.c_str());
            std::lock_guard<std::mutex> lock(mapDataMutex_);
            pendingMapData_ = std::move(data);
          } else {
            std::fprintf(stderr, "MapWidget: fetch failed or empty for %s\n",
                         url_str.c_str());
          }
        },
        86400 * 30); // Cache for a month
  }
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
  if (!state_->dxActive)
    return;

  LatLon de = state_->deLocation;
  LatLon dx = state_->dxLocation;

  auto path = Astronomy::calculateGreatCirclePath(de, dx, 250);
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
                                            2.5f, color);
        }
        segment.clear();
      }
    }
    segment.push_back(points[i]);
  }
  if (segment.size() >= 2) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, segment.data(),
                                      static_cast<int>(segment.size()), 2.5f,
                                      color);
  }
}

void MapWidget::renderNightOverlay(SDL_Renderer *renderer) {
  // Use higher point density for smoother fallback rendering
  auto terminator = Astronomy::calculateTerminator(sunLat_, sunLon_, 720);
  int N = static_cast<int>(terminator.size());
  if (N < 2)
    return;

  bool nightBelow = (sunLat_ >= 0);
  float edgeY = nightBelow ? static_cast<float>(mapRect_.y + mapRect_.h)
                           : static_cast<float>(mapRect_.y);

  SDL_Color nightColor = {0, 8, 24, 180}; // Deep midnight blue

  SDL_RenderSetClipRect(renderer, &mapRect_);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

#if SDL_VERSION_ATLEAST(2, 0, 18)
  std::vector<SDL_Vertex> verts(2 * N);
  for (int i = 0; i < N; ++i) {
    SDL_FPoint tp = latLonToScreen(terminator[i].lat, terminator[i].lon);
    verts[2 * i].position = {tp.x, tp.y};
    verts[2 * i].color = nightColor;
    verts[2 * i].tex_coord = {0, 0};

    verts[2 * i + 1].position = {tp.x, edgeY};
    verts[2 * i + 1].color = nightColor;
    verts[2 * i + 1].tex_coord = {0, 0};
  }

  std::vector<int> indices;
  indices.reserve((N - 1) * 6);
  for (int i = 0; i < N - 1; ++i) {
    int tl = 2 * i, bl = 2 * i + 1, tr = 2 * (i + 1), br = 2 * (i + 1) + 1;
    indices.push_back(tl);
    indices.push_back(bl);
    indices.push_back(tr);
    indices.push_back(tr);
    indices.push_back(bl);
    indices.push_back(br);
  }

  SDL_RenderGeometry(renderer, nullptr, verts.data(),
                     static_cast<int>(verts.size()), indices.data(),
                     static_cast<int>(indices.size()));
#else
  // Fallback: draw vertical slices from terminator to edge using solid Rects
  SDL_SetRenderDrawColor(renderer, nightColor.r, nightColor.g, nightColor.b,
                         nightColor.a);
  int iEdgeY = static_cast<int>(edgeY);
  for (int i = 0; i < N - 1; ++i) {
    SDL_FPoint p1 = latLonToScreen(terminator[i].lat, terminator[i].lon);
    SDL_FPoint p2 =
        latLonToScreen(terminator[i + 1].lat, terminator[i + 1].lon);

    if (std::abs(p2.x - p1.x) > mapRect_.w / 2.0f)
      continue;

    int x1 = static_cast<int>(std::round(p1.x));
    int x2 = static_cast<int>(std::round(p2.x));

    int start = std::min(x1, x2);
    int end = std::max(x1, x2);

    for (int x = start; x <= end; ++x) {
      float f = (x1 == x2) ? 0.0f : (float)(x - x1) / (x2 - x1);
      int y = static_cast<int>(std::round(p1.y + f * (p2.y - p1.y)));

      SDL_Rect r;
      r.x = x;
      r.w = 1;
      if (y < iEdgeY) {
        r.y = y;
        r.h = iEdgeY - y;
      } else {
        r.y = iEdgeY;
        r.h = y - iEdgeY;
      }
      SDL_RenderFillRect(renderer, &r);
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
      // std::fprintf(stderr,
      //              "MapWidget: applying pending map data (%zu bytes)...\n",
      //              pendingMapData_.size());
      texMgr_.loadFromMemory(renderer, MAP_KEY, pendingMapData_);
      pendingMapData_.clear();
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

  renderSatellite(renderer);
  renderSpotOverlay(renderer);
  renderDXClusterSpots(renderer);
  renderMarker(renderer, sunLat_, sunLon_, 255, 255, 0, MarkerShape::Circle,
               true);

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

  for (const auto &spot : data.spots) {
    int bandIdx = freqToBandIndex(spot.freqKhz);
    if (bandIdx < 0 || !data.selectedBands[bandIdx])
      continue;
    double lat, lon;
    if (!Astronomy::gridToLatLon(spot.receiverGrid, lat, lon))
      continue;
    const auto &bc = kBands[bandIdx].color;
    auto path = Astronomy::calculateGreatCirclePath(de, {lat, lon}, 100);
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
  for (const auto &spot : data.spots) {
    if (spot.txLat != 0.0 || spot.txLon != 0.0) {
      // Plot transmitter as a small white/grey circle
      renderMarker(renderer, spot.txLat, spot.txLon, 255, 255, 255,
                   MarkerShape::Circle, true);
    }
  }
  SDL_RenderSetClipRect(renderer, nullptr);
}

void MapWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  recalcMapRect();
}
