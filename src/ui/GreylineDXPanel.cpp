#include "GreylineDXPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

GreylineDXPanel::GreylineDXPanel(int x, int y, int w, int h,
                                 FontManager &fontMgr,
                                 std::shared_ptr<GreylineDXStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void GreylineDXPanel::update() {
  if (store_) {
    currentData_ = store_->get();
  }
}

// Format minutesToPeak as "+Xm Ys" / "-Xm Ys" countdown string.
static void formatCountdown(char *buf, int bufSize, double minutesToPeak) {
  char sign = (minutesToPeak >= 0) ? '+' : '-';
  double absMins = std::abs(minutesToPeak);
  int m = static_cast<int>(absMins);
  int s = static_cast<int>((absMins - m) * 60.0 + 0.5);
  if (s >= 60) { ++m; s = 0; }
  std::snprintf(buf, bufSize, "%c%dm%02ds", sign, m, s);
}

void GreylineDXPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Check if any entity is within 5 minutes of its peak (approaching)
  bool alertActive = false;
  for (const auto &e : currentData_.activeEntities) {
    if (e.minutesToPeak >= 0.0 && e.minutesToPeak < 5.0) {
      alertActive = true;
      break;
    }
  }

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border — pulse warning color when any entity is near peak
  SDL_Color borderColor = themes.border;
  if (alertActive && (SDL_GetTicks() / 500) % 2 == 0)
    borderColor = themes.warning;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b,
                         borderColor.a);
  SDL_RenderDrawRect(renderer, &rect);

  int titleH = 20;
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Greyline DX", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!currentData_.valid || currentData_.activeEntities.empty()) {
    cat->drawText(renderer, "No entities in greyline", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, themes.textDim,
                  FontStyle::Micro, true, false, true);
    return;
  }

  int pad = 4;
  int curY = y_ + titleH + pad;
  int rowH = (height_ - titleH - 2 * pad) / 7;
  if (rowH < 12) rowH = 12;

  int maxRows = std::max(1, (height_ - titleH - 2 * pad) / rowH - 1); // -1 for header
  maxScroll_ = std::max(0, (int)currentData_.activeEntities.size() - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);
  int startIdx = std::max(0, scrollOffset_);

  // Column offsets
  int colPX = x_ + pad;
  int colOX = x_ + 35;
  int colNX = x_ + 90;

  // Header
  cat->drawText(renderer, "PFX", colPX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  cat->drawText(renderer, "CNTDWN", colOX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  cat->drawText(renderer, "ENTITY", colNX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  curY += rowH;

  for (int ei = startIdx; ei < (int)currentData_.activeEntities.size(); ++ei) {
    const auto &entity = currentData_.activeEntities[ei];
    if (curY + rowH > y_ + height_ - pad)
      break;

    std::string pfx = entity.prefix.empty() ? "??" : entity.prefix;
    char oBuf[16];
    formatCountdown(oBuf, sizeof(oBuf), entity.minutesToPeak);

    // Approaching peak < 5 min: highlight row
    bool nearPeak = entity.minutesToPeak >= 0.0 && entity.minutesToPeak < 5.0;
    SDL_Color rowColor = nearPeak ? themes.warning
                       : (entity.isSunrise ? themes.warning : themes.info);
    SDL_Color cntColor = nearPeak ? themes.warning : themes.text;

    cat->drawText(renderer, pfx, colPX, curY + rowH / 2, themes.text,
                  FontStyle::Fast, false, false, true);
    cat->drawText(renderer, oBuf, colOX, curY + rowH / 2, cntColor,
                  FontStyle::Fast, false, false, true);

    // Entity name (truncated if needed)
    std::string name = entity.name;
    if (name.length() > 12)
      name = name.substr(0, 10) + "..";
    cat->drawText(renderer, name, colNX, curY + rowH / 2, rowColor,
                  FontStyle::Micro, false, false, true);

    curY += rowH;
  }
}

bool GreylineDXPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}

void GreylineDXPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

REGISTER_WIDGET("greyline_dx", "Greyline DX", true, false, {
  return std::make_unique<GreylineDXPanel>(0, 0, 0, 0, deps.fontMgr, deps.greylineDXStore);
})
