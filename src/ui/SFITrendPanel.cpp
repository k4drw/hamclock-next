#include "SFITrendPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include "WidgetRegistry.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>

SFITrendPanel::SFITrendPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<SFIHistoryStore> sfiHistoryStore,
                             std::shared_ptr<SolarDataStore> solarStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr),
      sfiHistoryStore_(std::move(sfiHistoryStore)),
      solarStore_(std::move(solarStore)) {}

void SFITrendPanel::update() {}

void SFITrendPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

// Color for an SFI value:
//   <80    → red (bands barely open)
//   80-119 → yellow (low activity)
//   120-159 → green (moderate-good)
//   ≥160   → cyan (excellent DX)
static SDL_Color sfiColor(int flux) {
  if (flux >= 160) return {0,   230, 220, 255};  // cyan
  if (flux >= 120) return {60,  200, 60,  255};  // green
  if (flux >= 80)  return {240, 220, 30,  255};  // yellow
  return               {220, 60,  60,  255};     // red
}

// Chart geometry helper — duplicated between render() and onMouseMove()
// so both methods refer to identical coordinates.
struct ChartGeom {
  int x, y, w, h;
  int yMin, yMax;
  bool valid() const { return w >= 30 && h >= 20; }
  int fluxToY(int flux) const {
    int clamped = std::max(yMin, std::min(yMax, flux));
    return y + h - (clamped - yMin) * h / (yMax - yMin);
  }
};

static ChartGeom chartGeom(int wx, int wy, int ww, int wh) {
  const int titleH  = 18;
  const int labelW  = 22;
  const int bottomH = 12;
  ChartGeom g;
  g.x    = wx + labelW;
  g.y    = wy + titleH + 2;
  g.w    = ww - labelW - 4;
  g.h    = wh - titleH - bottomH - 4;
  g.yMin = 60;
  g.yMax = 220;
  return g;
}

void SFITrendPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  // Title
  cat->drawText(renderer, "SFI 30-Day", x_ + 6, y_ + 4, themes.accent,
                FontStyle::MicroBold);

  // Current SFI — right-aligned, left of the maximize button (~20px margin)
  int currentSFI = 0;
  if (solarStore_) {
    SolarData solar = solarStore_->get();
    if (solar.valid)
      currentSFI = solar.sfi;
  }
  if (currentSFI > 0) {
    char valStr[16];
    std::snprintf(valStr, sizeof(valStr), "%d", currentSFI);
    SDL_Color col = sfiColor(currentSFI);
    cat->drawText(renderer, valStr, x_ + width_ - 20, y_ + 4,
                  col, FontStyle::SmallBold, false, true);
  }

  if (!sfiHistoryStore_ || !sfiHistoryStore_->hasData()) {
    cat->drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  auto pts = sfiHistoryStore_->getPoints();
  if (pts.size() < 2) {
    cat->drawText(renderer, "Loading...", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  ChartGeom g = chartGeom(x_, y_, width_, height_);
  if (!g.valid())
    return;

  // Band-viability reference lines
  struct RefLine { int sfi; const char *label; SDL_Color col; };
  static const RefLine refs[] = {
    {80,  "80",  {220, 60,  60,  100}},
    {120, "120", {240, 220, 30,  100}},
    {160, "160", {0,   200, 180, 100}},
  };
  for (const auto &r : refs) {
    if (r.sfi < g.yMin || r.sfi > g.yMax) continue;
    int ry = g.fluxToY(r.sfi);
    SDL_SetRenderDrawColor(renderer, r.col.r, r.col.g, r.col.b, r.col.a);
    SDL_RenderDrawLine(renderer, g.x, ry, g.x + g.w, ry);
    cat->drawText(renderer, r.label, x_ + 2, ry, r.col, FontStyle::Tiny,
                  false, false, true);
  }

  // Draw SFI line — color each segment by average flux
  int n = static_cast<int>(pts.size());
  for (int i = 1; i < n; i++) {
    float x1f = g.x + (float)(i - 1) * g.w / (n - 1);
    float x2f = g.x + (float)i       * g.w / (n - 1);
    int y1 = g.fluxToY(pts[i - 1].flux);
    int y2 = g.fluxToY(pts[i].flux);
    SDL_Color lc = sfiColor((pts[i - 1].flux + pts[i].flux) / 2);
    RenderUtils::drawThickLine(renderer, x1f, (float)y1, x2f, (float)y2,
                               2.0f, lc);
  }

  // X-axis date labels
  if (n > 0) {
    const auto frontDate = pts.front().date.size() >= 10
                               ? pts.front().date.substr(5, 5)  // MM-DD
                               : pts.front().date;
    const auto backDate  = pts.back().date.size() >= 10
                               ? pts.back().date.substr(5, 5)
                               : pts.back().date;
    int axisY = y_ + height_ - 11;
    cat->drawText(renderer, frontDate.c_str(), g.x,       axisY, themes.textDim, FontStyle::Tiny);
    cat->drawText(renderer, backDate.c_str(),  g.x + g.w, axisY, themes.textDim, FontStyle::Tiny, false, true);
  }

  if (tooltip_.visible)
    renderTooltip(renderer, fontMgr_);
}

void SFITrendPanel::onMouseMove(int mx, int my) {
  if (!sfiHistoryStore_ || !sfiHistoryStore_->hasData()) {
    tooltip_.visible = false;
    return;
  }
  auto pts = sfiHistoryStore_->getPoints();
  if (pts.size() < 2) {
    tooltip_.visible = false;
    return;
  }

  ChartGeom g = chartGeom(x_, y_, width_, height_);
  if (!g.valid() || mx < g.x || mx > g.x + g.w ||
      my < g.y || my > g.y + g.h) {
    tooltip_.visible = false;
    return;
  }

  // Find the nearest point by X
  int n = static_cast<int>(pts.size());
  float bestDist = 99999.0f;
  int bestIdx = -1;
  for (int i = 0; i < n; i++) {
    float px = g.x + (float)i * g.w / (n - 1);
    float d = std::abs(static_cast<float>(mx) - px);
    if (d < bestDist) {
      bestDist = d;
      bestIdx = i;
    }
  }

  if (bestIdx >= 0 && bestDist < 15.0f) {
    const auto &p = pts[bestIdx];
    char buf[48];
    std::snprintf(buf, sizeof(buf), "SFI %d  %s", p.flux, p.date.c_str());
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

REGISTER_WIDGET("sfi_trend", "SFI 30-Day", false, false, {
  return std::make_unique<SFITrendPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.sfiHistoryStore, deps.solarStore);
})
