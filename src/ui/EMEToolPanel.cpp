#include "EMEToolPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>
#include <ctime>

EMEToolPanel::EMEToolPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void EMEToolPanel::recomputeCurve() {
  curveBase_ = std::time(nullptr);
  // Floor to nearest 30-min boundary
  curveBase_ = (curveBase_ / kIntervalSec) * kIntervalSec;
  curve_.clear();
  curve_.reserve(kPoints);
  nextWindow_ = 0;

  for (int i = 0; i < kPoints; ++i) {
    std::time_t t = curveBase_ + i * kIntervalSec;
    double deAz, deEl, dxAz, dxEl;
    Astronomy::moonAzEl(deLat_, deLon_, t, deAz, deEl);
    Astronomy::moonAzEl(dxLat_, dxLon_, t, dxAz, dxEl);
    curve_.push_back({deEl, dxEl});
    if (nextWindow_ == 0 && deEl > 0.0 && dxEl > 0.0) {
      nextWindow_ = t;
    }
  }
}

void EMEToolPanel::update() {
  currentData_ = store_->get();
  // Recompute curve once per minute (60 000 ms cadence handled by caller)
  std::time_t now = std::time(nullptr);
  if (curve_.empty() || now - curveBase_ > kIntervalSec) {
    recomputeCurve();
  }
}

void EMEToolPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  if (!fontMgr_.ready())
    return;

  // Title
  int titleH = 20;
  cat->drawText(renderer, "EME Planning  (48h)", x_ + 10, y_ + 5,
                themes.accent, FontStyle::MicroBold);

  // --- Chart area ---
  // Reserve bottom portion for next-window text (2 lines)
  int infoH = 36;
  int chartX = x_ + 4;
  int chartY = y_ + titleH + 4;
  int chartW = width_ - 8;
  int chartH = height_ - titleH - infoH - 8;
  if (chartH < 20 || chartW < 20)
    return;

  // Chart background
  SDL_SetRenderDrawColor(renderer, 10, 15, 10, 255);
  SDL_Rect chartRect = {chartX, chartY, chartW, chartH};
  SDL_RenderFillRect(renderer, &chartRect);
  SDL_SetRenderDrawColor(renderer, 40, 60, 40, 255);
  SDL_RenderDrawRect(renderer, &chartRect);

  // Horizon line (0 deg) at midpoint of elevation range [-90..+90]
  int horizY = chartY + chartH / 2;
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawLine(renderer, chartX, horizY, chartX + chartW, horizY);

  // Draw hour labels every 6 hours (every 12 points)
  for (int i = 0; i <= kPoints; i += 12) {
    int px = chartX + (i * chartW) / kPoints;
    SDL_SetRenderDrawColor(renderer, 50, 60, 50, 255);
    SDL_RenderDrawLine(renderer, px, chartY, px, chartY + chartH);
    char hlabel[8];
    std::snprintf(hlabel, sizeof(hlabel), "%dh", i / 2);
    cat->drawText(renderer, hlabel, px + 2, chartY + 2,
                  {70, 90, 70, 255}, FontStyle::Caption);
  }

  if (curve_.size() < 2)
    return;

  // Helper: map elevation [-90..+90] to Y in chart
  auto elToY = [&](double el) -> int {
    // Centre at horizon, ±90 maps to ±chartH/2
    double clamped = std::max(-90.0, std::min(90.0, el));
    return horizY - static_cast<int>(clamped / 90.0 * (chartH * 0.5));
  };

  // Draw DE curve (green)
  SDL_SetRenderDrawColor(renderer, 0, 220, 80, 255);
  for (int i = 1; i < (int)curve_.size(); ++i) {
    int x1 = chartX + ((i - 1) * chartW) / (int)curve_.size();
    int x2 = chartX + (i * chartW) / (int)curve_.size();
    int y1 = elToY(curve_[i - 1].deEl);
    int y2 = elToY(curve_[i].deEl);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }

  // Draw DX curve (blue)
  SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
  for (int i = 1; i < (int)curve_.size(); ++i) {
    int x1 = chartX + ((i - 1) * chartW) / (int)curve_.size();
    int x2 = chartX + (i * chartW) / (int)curve_.size();
    int y1 = elToY(curve_[i - 1].dxEl);
    int y2 = elToY(curve_[i].dxEl);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }

  // Mark current time (vertical red tick at x=0)
  SDL_SetRenderDrawColor(renderer, 255, 60, 60, 255);
  SDL_RenderDrawLine(renderer, chartX, chartY, chartX, chartY + chartH);

  // --- Info section ---
  int infoY = chartY + chartH + 4;
  int centerX = x_ + width_ / 2;

  // Legend
  SDL_SetRenderDrawColor(renderer, 0, 220, 80, 255);
  SDL_Rect deBox = {x_ + 4, infoY + 4, 10, 8};
  SDL_RenderFillRect(renderer, &deBox);
  cat->drawText(renderer, "DE", x_ + 18, infoY + 4, {0, 220, 80, 255},
                FontStyle::Caption);

  SDL_SetRenderDrawColor(renderer, 80, 160, 255, 255);
  SDL_Rect dxBox = {x_ + 44, infoY + 4, 10, 8};
  SDL_RenderFillRect(renderer, &dxBox);
  cat->drawText(renderer, "DX", x_ + 58, infoY + 4, {80, 160, 255, 255},
                FontStyle::Caption);

  // Next mutual window
  if (nextWindow_ > 0) {
    std::tm tm{};
    Astronomy::portable_gmtime(&nextWindow_, &tm);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Next window: %02d:%02d UTC",
                  tm.tm_hour, tm.tm_min);
    cat->drawText(renderer, buf, centerX, infoY + 20,
                  SDL_Color{200, 255, 100, 255}, FontStyle::Caption, true);
  } else {
    cat->drawText(renderer, "No mutual window in 48h", centerX, infoY + 20,
                  themes.textDim, FontStyle::Caption, true);
  }
}

void EMEToolPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Force curve recomputation on next update()
  curve_.clear();
}

nlohmann::json EMEToolPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  if (!currentData_.valid)
    return j;
  j["de_elevation"] = currentData_.elevation;
  j["dx_elevation"] = currentData_.dx_elevation;
  j["mutual_window"] = currentData_.mutual_window;
  j["path_loss_db"] = currentData_.path_loss_db;
  j["distance_km"] = currentData_.distance_km;
  if (nextWindow_ > 0) {
    j["next_window_utc"] = nextWindow_;
  }
  return j;
}
