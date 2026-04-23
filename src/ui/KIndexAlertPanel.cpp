#include "KIndexAlertPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>

KIndexAlertPanel::KIndexAlertPanel(int x, int y, int w, int h,
                                   FontManager &fontMgr,
                                   std::shared_ptr<KIndexHistoryStore> store,
                                   std::shared_ptr<SolarDataStore> solarStore,
                                   const AppConfig *config)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      solarStore_(std::move(solarStore)), config_(config) {}

void KIndexAlertPanel::update() {}

void KIndexAlertPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

// Returns NOAA G-scale (0-5) for a given Kp
static int kpToGScale(float kp) {
  if (kp >= 9.0f) return 5;
  if (kp >= 8.0f) return 4;
  if (kp >= 7.0f) return 3;
  if (kp >= 6.0f) return 2;
  if (kp >= 5.0f) return 1;
  return 0;
}

SDL_Color KIndexAlertPanel::kpColor(float kp) {
  if (kp >= 6.0f) return {220, 50,  50,  255};  // red    — G2+
  if (kp >= 5.0f) return {255, 140, 0,   255};  // orange — G1
  if (kp >= 3.0f) return {240, 220, 30,  255};  // yellow — moderate
  return              {60,  200, 60,  255};      // green  — quiet
}

void KIndexAlertPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  // Reserve right margin for the maximize button (~16px wide at x_+width_-18)
  const int rightMargin = 20;
  const int titleH = 18;

  cat->drawText(renderer, "K-Index", x_ + 6, y_ + 4, themes.accent,
                FontStyle::MicroBold);

  if (!store_ || !store_->hasData()) {
    cat->drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  float current = store_->getCurrent();
  int gScale = kpToGScale(current);
  SDL_Color col = kpColor(current);

  // Kp and G-scale
  char kpGStr[24];
  std::snprintf(kpGStr, sizeof(kpGStr), "%.1f G%d", (double)current, gScale);
  cat->drawText(renderer, kpGStr, x_ + width_ - rightMargin, y_ + 3,
                col, FontStyle::SmallBold, false, true);

  // A-index below the title if space allows
  if (solarStore_) {
    SolarData sd = solarStore_->get();
    if (sd.valid) {
      char aStr[16];
      std::snprintf(aStr, sizeof(aStr), "A-idx %d", sd.a_index);
      cat->drawText(renderer, aStr, x_ + 6, y_ + titleH, themes.textDim,
                    FontStyle::Micro);
    }
  }

  // Alert border if K >= threshold
  if (config_ && current >= config_->kIndexAlertThreshold) {
    SDL_SetRenderDrawColor(renderer, 220, 50, 50, 255); // red
    SDL_Rect r = {x_, y_, width_, height_};
    SDL_RenderDrawRect(renderer, &r);
    r.x++; r.y++; r.w -= 2; r.h -= 2;
    SDL_RenderDrawRect(renderer, &r);
  }

  // --- 24-hour bar chart ---
  auto history = store_->getHistory();

  // Left margin for K-level legend labels; bars start after it
  const int axisW = 16;
  int chartX = x_ + axisW;
  int chartY = y_ + titleH + 4;
  int chartW = width_ - axisW - 4;
  int chartH = height_ - titleH - 8;

  if (chartW < 20 || chartH < 20)
    return;

  // Reference line at K=5 (G1 threshold) with legend label
  int refY5 = chartY + chartH - static_cast<int>(5.0f * chartH / 9.0f);
  SDL_SetRenderDrawColor(renderer, 255, 140, 0, 100);
  SDL_RenderDrawLine(renderer, chartX, refY5, chartX + chartW, refY5);
  cat->drawText(renderer, "K5", x_ + 2, refY5, {255, 140, 0, 200},
                FontStyle::Tiny, false, false, true);

  // Reference line at K=3 (moderate) with legend label
  int refY3 = chartY + chartH - static_cast<int>(3.0f * chartH / 9.0f);
  SDL_SetRenderDrawColor(renderer, 240, 220, 30, 80);
  SDL_RenderDrawLine(renderer, chartX, refY3, chartX + chartW, refY3);
  cat->drawText(renderer, "K3", x_ + 2, refY3, {240, 220, 30, 200},
                FontStyle::Tiny, false, false, true);

  // Filter to last 24 hours
  auto now = std::chrono::system_clock::now();
  auto cutoff = now - std::chrono::hours(24);
  std::vector<KIndexPoint> pts24;
  for (const auto &p : history) {
    if (p.timestamp >= cutoff)
      pts24.push_back(p);
  }

  if (pts24.empty()) {
    cat->drawText(renderer, "Collecting...", chartX + chartW / 2,
                  chartY + chartH / 2, themes.textDim, FontStyle::Tiny, true, false, true);
    return;
  }

  // Draw bars spanning 24h window
  int n = static_cast<int>(pts24.size());
  float barW = static_cast<float>(chartW) / std::max(n, 24);
  if (barW < 1.0f) barW = 1.0f;
  int gapPx = barW > 3 ? 1 : 0;

  for (int i = 0; i < n; i++) {
    const auto &p = pts24[i];
    auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - p.timestamp).count();
    float ageFrac = static_cast<float>(ageMs) / (24.0f * 3600.0f * 1000.0f);
    int bx = chartX + static_cast<int>((1.0f - ageFrac) * chartW - barW);
    float kpClamped = std::min(p.kp, 9.0f);
    int barH = static_cast<int>(kpClamped * chartH / 9.0f);
    if (barH < 1) barH = 1;

    SDL_Rect bar{bx, chartY + chartH - barH,
                 static_cast<int>(barW) - gapPx, barH};
    SDL_Color bc = kpColor(p.kp);
    SDL_SetRenderDrawColor(renderer, bc.r, bc.g, bc.b, 200);
    SDL_RenderFillRect(renderer, &bar);
  }

  // Trend arrow (compare last 3 readings vs previous 3)
  if (n >= 6) {
    float recent = 0, earlier = 0;
    for (int i = n - 3; i < n; i++)     recent  += pts24[i].kp;
    for (int i = n - 6; i < n - 3; i++) earlier += pts24[i].kp;
    recent /= 3.0f; earlier /= 3.0f;

    const char *arrow = (recent > earlier + 0.3f) ? "\xe2\x96\xb2" :   // ▲
                        (recent < earlier - 0.3f) ? "\xe2\x96\xbc" :   // ▼
                                                    "\xe2\x96\xb6";     // ▶
    SDL_Color arrowCol = (recent > earlier + 0.3f) ? SDL_Color{220, 80, 80, 255} :
                         (recent < earlier - 0.3f) ? SDL_Color{80, 200, 80, 255} :
                                                     themes.textDim;
    cat->drawText(renderer, arrow, x_ + 6, y_ + titleH + 2, arrowCol,
                  FontStyle::Tiny);
  }
}

REGISTER_WIDGET("kindex_trend", "K-Index Alert", false, false, {
  return std::make_unique<KIndexAlertPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.kIndexHistoryStore, deps.solarStore, &deps.appCfg);
})
