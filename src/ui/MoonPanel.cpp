#include "MoonPanel.h"
#include "FontCatalog.h"
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MoonPanel::MoonPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void MoonPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;
}

void MoonPanel::drawMoon(SDL_Renderer *renderer, int cx, int cy, int r,
                         double phase) {
  // phase: 0.0 (New) -> 0.5 (Full) -> 1.0 (New)

  // 1. Draw background circle (dark shadow region)
  SDL_SetRenderDrawColor(renderer, 30, 30, 45, 255);
  for (int dy = -r; dy <= r; ++dy) {
    int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }

  // 2. Linear interpolate the illuminated part
  SDL_SetRenderDrawColor(renderer, 240, 240, 210, 255);
  double s = 2.0 * phase;
  for (int dy = -r; dy <= r; ++dy) {
    double dx = std::sqrt(r * r - dy * dy);
    if (s <= 1.0) {
      // New -> Full (lit from right)
      double term = (1.0 - 2.0 * s) * dx;
      SDL_RenderDrawLine(renderer, cx + static_cast<int>(term), cy + dy,
                         cx + static_cast<int>(dx), cy + dy);
    } else {
      // Full -> New (lit from left)
      double term = (3.0 - 2.0 * s) * dx;
      SDL_RenderDrawLine(renderer, cx - static_cast<int>(dx), cy + dy,
                         cx + static_cast<int>(term), cy + dy);
    }
  }
}

void MoonPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Background
  SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "No Moon Data", x_ + 10, y_ + height_ / 2 - 8,
                      {150, 150, 150, 255}, valueFontSize_);
    return;
  }

  // Centered layout
  int moonR = std::min(width_, height_) / 3 - 5;
  if (moonR > 40)
    moonR = 40;

  int moonY = y_ + moonR + 10;
  int centerX = x_ + width_ / 2;

  drawMoon(renderer, centerX, moonY, moonR, currentData_.phase);

  // Text labels
  int textY = moonY + moonR + 10;
  fontMgr_.drawText(renderer, currentData_.phaseName, centerX, textY,
                    {255, 255, 255, 255}, labelFontSize_, true, true);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f%% Illum", currentData_.illumination);
  fontMgr_.drawText(renderer, buf, centerX, textY + labelFontSize_ + 4,
                    {200, 200, 200, 255}, valueFontSize_, false, true);
}

void MoonPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  valueFontSize_ = cat->ptSize(FontStyle::Fast);

  if (h > 120) {
    labelFontSize_ = cat->ptSize(FontStyle::SmallBold);
    valueFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}
