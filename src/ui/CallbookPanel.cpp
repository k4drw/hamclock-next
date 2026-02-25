#include "CallbookPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

CallbookPanel::CallbookPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<CallbookStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void CallbookPanel::update() { currentData_ = store_->get(); }

void CallbookPanel::render(SDL_Renderer *renderer) {
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

  if (!currentData_.valid) {
    fontMgr_.drawText(renderer, "NO CALLSIGN DATA", x_ + width_ / 2,
                      y_ + height_ / 2, themes.textDim, infoSize_, false, true);
    return;
  }

  int titleH = 20;
  fontMgr_.drawText(renderer, "Callbook", x_ + 10, y_ + 5, themes.accent, 10,
                    true);

  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  // Callsign & Name
  fontMgr_.drawText(renderer, currentData_.callsign, centerX, curY,
                    themes.accent, titleSize_, true, true);
  curY += titleSize_ + 4;

  fontMgr_.drawText(renderer, currentData_.name, centerX, curY, themes.text,
                    nameSize_, false, true);
  curY += nameSize_ + 15;

  // Location / Grid
  char locBuf[64];
  std::snprintf(locBuf, sizeof(locBuf), "%s, %s", currentData_.city.c_str(),
                currentData_.country.empty() ? "USA"
                                             : currentData_.country.c_str());
  fontMgr_.drawText(renderer, locBuf, centerX, curY, themes.text, infoSize_,
                    false, true);
  curY += infoSize_ + 4;

  std::snprintf(locBuf, sizeof(locBuf), "Grid: %s", currentData_.grid.c_str());
  fontMgr_.drawText(renderer, locBuf, centerX, curY, {0, 255, 150, 255},
                    infoSize_, true, true);
  curY += infoSize_ + 15;

  // QSL Info (Badges)
  int badgeX = x_ + 20;
  if (currentData_.lotw) {
    fontMgr_.drawText(renderer, "[LoTW]", badgeX, curY, {200, 200, 255, 255},
                      infoSize_ - 2);
    badgeX += 60;
  }
  if (currentData_.eqsl) {
    fontMgr_.drawText(renderer, "[eQSL]", badgeX, curY, {200, 255, 200, 255},
                      infoSize_ - 2);
  }

  // Attribution
  fontMgr_.drawText(renderer, currentData_.source, x_ + width_ - 5,
                    y_ + height_ - 15, themes.textDim, 9, false, false);
}

void CallbookPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleSize_ = cat->ptSize(FontStyle::MediumBold);
  nameSize_ = cat->ptSize(FontStyle::SmallRegular);
  infoSize_ = cat->ptSize(FontStyle::Micro);
}
