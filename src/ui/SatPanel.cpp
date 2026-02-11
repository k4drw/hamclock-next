#include "SatPanel.h"
#include "FontCatalog.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <vector>

static constexpr double kPi = 3.14159265358979323846;
static constexpr double kDeg2Rad = kPi / 180.0;

static const char *kCompassLabels[8] = {"N", "NE", "E", "SE",
                                        "S", "SW", "W", "NW"};

SatPanel::SatPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void SatPanel::destroyCache() {
  for (int i = 0; i < kNumLines; ++i) {
    if (lineTex_[i]) {
      SDL_DestroyTexture(lineTex_[i]);
      lineTex_[i] = nullptr;
    }
    lastLineText_[i].clear();
    lastLineFontSize_[i] = 0;
  }
  for (int i = 0; i < kNumCompass; ++i) {
    if (compassTex_[i]) {
      SDL_DestroyTexture(compassTex_[i]);
      compassTex_[i] = nullptr;
    }
  }
  lastCompassFontSize_ = 0;
}

void SatPanel::update() {
  if (!hasPredictor()) {
    lineText_[0] = "No satellite";
    lineText_[1] = "selected";
    lineText_[2].clear();
    lineText_[3].clear();
    passTrack_.clear();
    satAboveHorizon_ = false;
    return;
  }

  // Throttle: recalculate at most once per second
  std::time_t now = std::time(nullptr);
  if (now == lastUpdate_)
    return;
  lastUpdate_ = now;

  // Line 0: Satellite name (centered, large)
  lineText_[0] = predictor_->satName();

  // Current observation
  SatObservation obs = predictor_->observe();
  currentPos_ = {obs.azimuth, obs.elevation};
  satAboveHorizon_ = (obs.elevation > 0.0);

  // Next pass
  SatPass pass = predictor_->nextPass();

  // Line 1: "Rise in Xh:XX @ AZZ" or "Up for Xh:XX"
  char buf[64];
  if (pass.aosTime > 0) {
    if (obs.elevation > 0.0) {
      // Currently overhead — show time until LOS
      long remain = static_cast<long>(pass.losTime - now);
      if (remain < 0)
        remain = 0;
      int mins = static_cast<int>(remain / 60);
      int secs = static_cast<int>(remain % 60);
      std::snprintf(buf, sizeof(buf), "Set in  %d:%02d @ %.0f", mins, secs,
                    pass.losAz);
    } else {
      long until = static_cast<long>(pass.aosTime - now);
      if (until < 0)
        until = 0;
      int hrs = static_cast<int>(until / 3600);
      int mins = static_cast<int>((until % 3600) / 60);
      if (hrs > 0) {
        std::snprintf(buf, sizeof(buf), "Rise in  %dh%02d @ %.0f", hrs, mins,
                      pass.aosAz);
      } else {
        std::snprintf(buf, sizeof(buf), "Rise in  %d:%02d @ %.0f", mins,
                      static_cast<int>(until % 60), pass.aosAz);
      }
    }
    lineText_[1] = buf;
  } else {
    lineText_[1] = "No pass found";
  }

  // Line 2: "Az: NNN    El: NN"
  std::snprintf(buf, sizeof(buf), "Az: %.0f    El: %.0f", obs.azimuth,
                obs.elevation);
  lineText_[2] = buf;

  // Line 3: "TLE Age X.X days"
  double age = predictor_->tleAgeDays();
  if (age >= 0.0) {
    std::snprintf(buf, sizeof(buf), "TLE Age %.1f days", age);
    lineText_[3] = buf;
  } else {
    lineText_[3].clear();
  }

  // Build pass trajectory for polar plot
  passTrack_.clear();
  if (pass.aosTime > 0 && pass.losTime > pass.aosTime) {
    long duration = static_cast<long>(pass.losTime - pass.aosTime);
    int steps = std::max(30, static_cast<int>(duration / 10));
    passTrack_.reserve(steps + 1);
    for (int s = 0; s <= steps; ++s) {
      std::time_t t = pass.aosTime + (duration * s) / steps;
      SatObservation o = predictor_->observeAt(t);
      passTrack_.push_back({o.azimuth, o.elevation});
    }
  }
}

void SatPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Clip to widget bounds
  SDL_Rect clip = {x_, y_, width_, height_};
  SDL_RenderSetClipRect(renderer, &clip);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &clip);

  int pad = 2;
  int curY = y_ + pad;

  // Text colors
  SDL_Color white = {255, 255, 255, 255};
  SDL_Color gray = {180, 180, 180, 255};

  // Render text lines (centered)
  for (int i = 0; i < kNumLines; ++i) {
    if (lineText_[i].empty())
      continue;

    bool needRedraw = !lineTex_[i] || (lineText_[i] != lastLineText_[i]) ||
                      (lineFontSize_[i] != lastLineFontSize_[i]);
    if (needRedraw) {
      if (lineTex_[i]) {
        SDL_DestroyTexture(lineTex_[i]);
        lineTex_[i] = nullptr;
      }
      SDL_Color c = (i == 0) ? white : gray; // name white, rest gray
      lineTex_[i] = fontMgr_.renderText(
          renderer, lineText_[i], c, lineFontSize_[i], &lineW_[i], &lineH_[i]);
      lastLineText_[i] = lineText_[i];
      lastLineFontSize_[i] = lineFontSize_[i];
    }
    if (lineTex_[i]) {
      int tx = x_ + (width_ - lineW_[i]) / 2; // centered
      SDL_Rect dst = {tx, curY, lineW_[i], lineH_[i]};
      SDL_RenderCopy(renderer, lineTex_[i], nullptr, &dst);
      curY += lineH_[i] + 1;
    }
  }

  // Polar plot fills remaining space below text
  int plotTop = curY + pad;
  int plotBottom = y_ + height_ - pad;
  int plotH = plotBottom - plotTop;
  int plotW = width_ - 2 * pad;
  if (plotH > 10 && plotW > 10 && hasPredictor()) {
    int radius = std::min(plotW, plotH) / 2 - 2;
    int cx = x_ + width_ / 2;
    int cy = plotTop + plotH / 2;
    renderPolarPlot(renderer, cx, cy, radius);
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void SatPanel::renderPolarPlot(SDL_Renderer *renderer, int cx, int cy,
                               int radius) {
  // --- Compass labels (outside the circle) ---
  if (compassFontSize_ != lastCompassFontSize_) {
    for (int i = 0; i < kNumCompass; ++i) {
      if (compassTex_[i]) {
        SDL_DestroyTexture(compassTex_[i]);
        compassTex_[i] = nullptr;
      }
    }
    lastCompassFontSize_ = compassFontSize_;
  }

  SDL_Color dimGray = {120, 120, 120, 255};
  for (int i = 0; i < kNumCompass; ++i) {
    if (!compassTex_[i]) {
      compassTex_[i] =
          fontMgr_.renderText(renderer, kCompassLabels[i], dimGray,
                              compassFontSize_, &compassW_[i], &compassH_[i]);
    }
    if (compassTex_[i]) {
      double angle = i * 45.0 * kDeg2Rad; // N=0, NE=45, E=90, ...
      int labelDist = radius + 2;
      int lx =
          cx + static_cast<int>(labelDist * std::sin(angle)) - compassW_[i] / 2;
      int ly =
          cy - static_cast<int>(labelDist * std::cos(angle)) - compassH_[i] / 2;
      SDL_Rect dst = {lx, ly, compassW_[i], compassH_[i]};
      SDL_RenderCopy(renderer, compassTex_[i], nullptr, &dst);
    }
  }

  // --- Concentric elevation circles (0, 30, 60 degrees) ---
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  for (int elev = 0; elev <= 60; elev += 30) {
    int r = radius * (90 - elev) / 90;
    // Draw circle as line segments
    constexpr int kSegs = 72;
    SDL_Point prev{};
    for (int s = 0; s <= kSegs; ++s) {
      double theta = 2.0 * kPi * s / kSegs;
      int px = cx + static_cast<int>(r * std::cos(theta));
      int py = cy + static_cast<int>(r * std::sin(theta));
      if (s > 0) {
        RenderUtils::drawThickLine(
            renderer, static_cast<float>(prev.x), static_cast<float>(prev.y),
            static_cast<float>(px), static_cast<float>(py), 1.0f,
            {60, 60, 60, 255});
      }
      prev = {px, py};
    }
  }

  // --- Radial lines (every 45 degrees) ---
  for (int i = 0; i < 8; ++i) {
    double angle = i * 45.0 * kDeg2Rad;
    int ex = cx + static_cast<int>(radius * std::sin(angle));
    int ey = cy - static_cast<int>(radius * std::cos(angle));
    RenderUtils::drawThickLine(renderer, static_cast<float>(cx),
                               static_cast<float>(cy), static_cast<float>(ex),
                               static_cast<float>(ey), 1.0f, {60, 60, 60, 255});
  }

  // --- Pass trajectory arc (green) ---
  if (passTrack_.size() >= 2) {
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    for (size_t i = 1; i < passTrack_.size(); ++i) {
      auto project = [&](const AzElPoint &p) -> SDL_Point {
        double r = radius * (90.0 - p.el) / 90.0;
        int px = cx + static_cast<int>(r * std::sin(p.az * kDeg2Rad));
        int py = cy - static_cast<int>(r * std::cos(p.az * kDeg2Rad));
        return {px, py};
      };
      SDL_Point p1 = project(passTrack_[i - 1]);
      SDL_Point p2 = project(passTrack_[i]);
      RenderUtils::drawThickLine(
          renderer, static_cast<float>(p1.x), static_cast<float>(p1.y),
          static_cast<float>(p2.x), static_cast<float>(p2.y), 2.0f,
          {0, 200, 0, 255});
    }
  }

  // --- Current satellite position (if above horizon) ---
  if (satAboveHorizon_) {
    double r = radius * (90.0 - currentPos_.el) / 90.0;
    int sx = cx + static_cast<int>(r * std::sin(currentPos_.az * kDeg2Rad));
    int sy = cy - static_cast<int>(r * std::cos(currentPos_.az * kDeg2Rad));

    // Filled circle marker
    float markerR = std::max(2.0f, radius / 20.0f);
    RenderUtils::drawCircle(renderer, static_cast<float>(sx),
                            static_cast<float>(sy), markerR, {0, 255, 0, 255});

    // Label: elevation angle
    char elBuf[16];
    std::snprintf(elBuf, sizeof(elBuf), "%.0f%c", currentPos_.el, '\xB0');
    SDL_Color green = {0, 255, 0, 255};
    fontMgr_.drawText(renderer, elBuf, sx + markerR + 2,
                      sy - compassFontSize_ / 2, green, compassFontSize_);
  }
}

void SatPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

  nameFontSize_ = std::clamp(h / 12, 8, cat->ptSize(FontStyle::SmallRegular));
  infoFontSize_ = cat->ptSize(FontStyle::Fast);
  compassFontSize_ = infoFontSize_;

  lineFontSize_[0] = nameFontSize_; // Sat name
  lineFontSize_[1] = infoFontSize_; // Rise in / Set in
  lineFontSize_[2] = infoFontSize_; // Az / El
  lineFontSize_[3] = infoFontSize_; // TLE Age

  destroyCache();
}
