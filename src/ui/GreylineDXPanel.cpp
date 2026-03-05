#include "GreylineDXPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
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

void GreylineDXPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  auto *cat = fontMgr_.catalog();
  int titleH = 20;
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

  // Column offsets
  int colPX = x_ + pad;
  int colOX = x_ + 35;
  int colNX = x_ + 75;

  // Header
  cat->drawText(renderer, "PFX", colPX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  cat->drawText(renderer, "OFFSET", colOX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  cat->drawText(renderer, "ENTITY", colNX, curY + rowH / 2, themes.textDim,
                FontStyle::Tiny, false, false, true);
  curY += rowH;

  int count = 0;
  for (const auto &entity : currentData_.activeEntities) {
    if (curY + rowH > y_ + height_ - pad)
      break;

    std::string pfx = entity.prefix.empty() ? "??" : entity.prefix;
    char oBuf[16];
    std::snprintf(oBuf, sizeof(oBuf), "%c%02.0fm",
                  entity.minutesToPeak >= 0 ? '+' : '-',
                  std::abs(entity.minutesToPeak));

    SDL_Color valColor = entity.isSunrise ? themes.warning : themes.info;

    // Draw row
    cat->drawText(renderer, pfx, colPX, curY + rowH / 2, themes.text,
                  FontStyle::Fast, false, false, true);
    cat->drawText(renderer, oBuf, colOX, curY + rowH / 2, themes.text,
                  FontStyle::Fast, false, false, true);

    // Entity name (truncated if needed)
    std::string name = entity.name;
    if (name.length() > 14)
      name = name.substr(0, 12) + "..";
    cat->drawText(renderer, name, colNX, curY + rowH / 2, valColor,
                  FontStyle::Micro, false, false, true);

    curY += rowH;
    if (++count >= 6)
      break;
  }
}

void GreylineDXPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}
