#include "EMEToolPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cstdio>
#include <ctime>

EMEToolPanel::EMEToolPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           TextureManager &texMgr,
                           std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), store_(store) {}

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

void EMEToolPanel::onMouseMove(int mx, int my) {
  if (mx >= chartRect_.x && mx < chartRect_.x + chartRect_.w &&
      my >= chartRect_.y && my < chartRect_.y + chartRect_.h) {
    tooltip_.visible = true;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.index =
        std::clamp((mx - chartRect_.x) * kPoints / chartRect_.w, 0, kPoints - 1);
  } else {
    tooltip_.visible = false;
  }
}

void EMEToolPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  renderChrome(renderer);

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

  chartRect_ = {chartX, chartY, chartW, chartH};

  // Chart background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, themes.bg.a);
  SDL_RenderFillRect(renderer, &chartRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &chartRect_);

  // Horizon line (0 deg) at midpoint of elevation range [-90..+90]
  int horizY = chartY + chartH / 2;
  SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g, themes.textDim.b, themes.textDim.a);
  SDL_RenderDrawLine(renderer, chartX, horizY, chartX + chartW, horizY);

  // Draw hour labels every 6 hours (every 12 points)
  for (int i = 0; i <= kPoints; i += 12) {
    int px = chartX + (i * chartW) / kPoints;
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
    SDL_RenderDrawLine(renderer, px, chartY, px, chartY + chartH);
    char hlabel[8];
    std::snprintf(hlabel, sizeof(hlabel), "%dh", i / 2);
    cat->drawText(renderer, hlabel, px + 2, chartY + 2,
                  themes.textDim, FontStyle::Caption);
  }

  if (curve_.size() < 2)
    return;

  // Helper: map elevation [-90..+90] to Y in chart
  auto elToY = [&](double el) -> float {
    // Centre at horizon, ±90 maps to ±chartH/2
    double clamped = std::max(-90.0, std::min(90.0, el));
    return (float)horizY - static_cast<float>(clamped / 90.0 * (chartH * 0.5f));
  };

  SDL_Texture *lineTex = texMgr_.get("line_aa");
  std::vector<SDL_FPoint> dePoints, dxPoints;
  dePoints.reserve(curve_.size());
  dxPoints.reserve(curve_.size());

  for (int i = 0; i < (int)curve_.size(); ++i) {
    float px = (float)chartX + (float)(i * chartW) / (float)kPoints;
    dePoints.push_back({px, elToY(curve_[i].deEl)});
    dxPoints.push_back({px, elToY(curve_[i].dxEl)});
  }

  // Draw DE curve (green)
  RenderUtils::drawPolylineTextured(renderer, lineTex, dePoints.data(),
                                    (int)dePoints.size(), 2.0f,
                                    themes.success);

  // Draw DX curve (blue)
  RenderUtils::drawPolylineTextured(renderer, lineTex, dxPoints.data(),
                                    (int)dxPoints.size(), 2.0f,
                                    themes.accent);

  // Mark current time (vertical red tick at x=0)
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, themes.danger.a);
  SDL_RenderDrawLine(renderer, chartX, chartY, chartX, chartY + chartH);

  // Tooltip
  if (tooltip_.visible && tooltip_.index >= 0 &&
      tooltip_.index < (int)curve_.size()) {
    int idx = tooltip_.index;
    float tx = dePoints[idx].x;
    SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 100);
    SDL_RenderDrawLine(renderer, (int)tx, chartY, (int)tx, chartY + chartH);

    std::time_t t = curveBase_ + idx * kIntervalSec;
    std::tm utc{};
    Astronomy::portable_gmtime(&t, &utc);

    char tbuf[64];
    std::snprintf(tbuf, sizeof(tbuf), "%02d:%02dZ DE:%.0f DX:%.0f", utc.tm_hour,
                  utc.tm_min, curve_[idx].deEl, curve_[idx].dxEl);

    int tw, th;
    cat->renderText(renderer, tbuf, themes.text, FontStyle::Micro, &tw,
                    &th);
    SDL_Rect tipRect = {tooltip_.x + 10, tooltip_.y - th - 5, tw + 10, th + 6};
    if (tipRect.x + tipRect.w > x_ + width_)
      tipRect.x = tooltip_.x - tipRect.w - 10;
    if (tipRect.y < y_)
      tipRect.y = tooltip_.y + 10;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
    SDL_RenderFillRect(renderer, &tipRect);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
    SDL_RenderDrawRect(renderer, &tipRect);
    cat->drawText(renderer, tbuf, tipRect.x + 5, tipRect.y + tipRect.h / 2,
                  themes.text, FontStyle::Micro, false, false, true);
  }

  // --- Info section ---
  int infoY = chartY + chartH + 4;
  int centerX = x_ + width_ / 2;

  // Legend
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, themes.success.a);
  SDL_Rect deBox = {x_ + 4, infoY + 4, 10, 8};
  SDL_RenderFillRect(renderer, &deBox);
  cat->drawText(renderer, "DE", x_ + 18, infoY + 8, themes.success,
                FontStyle::Caption, false, false, true);

  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, themes.accent.a);
  SDL_Rect dxBox = {x_ + 44, infoY + 4, 10, 8};
  SDL_RenderFillRect(renderer, &dxBox);
  cat->drawText(renderer, "DX", x_ + 58, infoY + 8, themes.accent,
                FontStyle::Caption, false, false, true);

  // Next mutual window
  if (nextWindow_ > 0) {
    std::tm tm{};
    Astronomy::portable_gmtime(&nextWindow_, &tm);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "Next window: %02d:%02d UTC",
                  tm.tm_hour, tm.tm_min);
    cat->drawText(renderer, buf, centerX, infoY + 22,
                  themes.success, FontStyle::Caption, true,
                  false, true);
  } else {
    cat->drawText(renderer, "No mutual window in 48h", centerX, infoY + 22,
                  themes.textDim, FontStyle::Caption, true, false, true);
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
