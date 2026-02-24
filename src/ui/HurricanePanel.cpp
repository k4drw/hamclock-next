#include "HurricanePanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

HurricanePanel::HurricanePanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<HurricaneStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void HurricanePanel::update() { currentData_ = store_->get(); }

SDL_Color HurricanePanel::categoryColor(int category) {
  switch (category) {
  case 5:
    return {255, 0, 0, 255}; // Extreme danger — red
  case 4:
    return {255, 60, 0, 255};
  case 3:
    return {255, 140, 0, 255};
  case 2:
    return {255, 220, 0, 255};
  case 1:
    return {255, 255, 80, 255};
  default:
    return {180, 180, 255, 255}; // TD or TS
  }
}

void HurricanePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int centerX = x_ + width_ / 2;
  int pad = 6;
  int curY = y_ + pad;

  fontMgr_.drawText(renderer, "Tropics", centerX, curY, themes.accent,
                    titleFontSize_, true, true);
  curY += titleFontSize_ + 6;

  if (!currentData_.valid) {
    fontMgr_.drawText(renderer, "Loading...", centerX, y_ + height_ / 2,
                      themes.textDim, detailFontSize_, false, true);
    return;
  }

  if (currentData_.storms.empty()) {
    fontMgr_.drawText(renderer, "No active storms", centerX, y_ + height_ / 2,
                      themes.success, detailFontSize_, false, true);
    return;
  }

  // Each storm block: name+category | wind | pressure | position
  int blockH = nameFontSize_ + detailFontSize_ * 2 + 8;
  int maxBlocks = (height_ - (curY - y_) - pad) / blockH;
  int startIdx = std::max(0, scrollOffset_);
  int endIdx = std::min((int)currentData_.storms.size(), startIdx + maxBlocks);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &s = currentData_.storms[i];
    SDL_Color col = categoryColor(s.category);

    // Storm name + category
    char nameLabel[32];
    if (s.category >= 1)
      std::snprintf(nameLabel, sizeof(nameLabel), "%s (Cat %d)", s.name.c_str(),
                    s.category);
    else
      std::snprintf(nameLabel, sizeof(nameLabel), "%s (TS/TD)", s.name.c_str());
    fontMgr_.drawText(renderer, nameLabel, x_ + pad, curY, col, nameFontSize_,
                      true, false);

    curY += nameFontSize_ + 2;

    // Wind + pressure
    char wBuf[48];
    std::snprintf(wBuf, sizeof(wBuf), "%dkt  %dmb  %.1fN %.1f%c",
                  s.maxWindKt, s.pressureMb, std::abs(s.lat),
                  std::abs(s.lon), s.lon < 0 ? 'W' : 'E');
    fontMgr_.drawText(renderer, wBuf, x_ + pad, curY, themes.textDim,
                      detailFontSize_, false, false);
    curY += detailFontSize_ + 2;

    // Movement
    if (!s.movement.empty()) {
      std::string mv = s.movement;
      if (mv.size() > 28)
        mv = mv.substr(0, 27) + "~";
      fontMgr_.drawText(renderer, mv, x_ + pad, curY, themes.textDim,
                        detailFontSize_, false, false);
    }
    curY += detailFontSize_ + 4;

    if (i < endIdx - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, 100);
      SDL_RenderDrawLine(renderer, x_ + pad, curY - 2, x_ + width_ - pad,
                         curY - 2);
    }
  }

  if ((int)currentData_.storms.size() > maxBlocks) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)currentData_.storms.size());
    fontMgr_.drawText(renderer, buf, x_ + width_ - pad,
                      y_ + height_ - pad - detailFontSize_, themes.textDim,
                      detailFontSize_, false, true);
  }
}

void HurricanePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  nameFontSize_ = cat->ptSize(FontStyle::Fast);
  detailFontSize_ = cat->ptSize(FontStyle::Micro);
  scrollOffset_ = 0;
}

bool HurricanePanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::max(0, scrollOffset_ - scrollY);
  return true;
}
