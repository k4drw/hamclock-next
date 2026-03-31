#include "TropoPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

namespace HamClock {

TropoPanel::TropoPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void TropoPanel::updateData(const TropoData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

SDL_Color TropoPanel::getLevelColor(TropoLevel l) {
  switch (l) {
    case TropoLevel::Insignificant: return {100, 100, 100, 255};
    case TropoLevel::VeryPoor:      return {150, 100, 100, 255};
    case TropoLevel::Poor:          return {200, 150, 100, 255};
    case TropoLevel::Fair:          return {200, 200, 100, 255};
    case TropoLevel::Good:          return {100, 200, 100, 255};
    case TropoLevel::VeryGood:      return {100, 255, 100, 255};
    case TropoLevel::Strong:        return {100, 255, 200, 255};
    case TropoLevel::VeryStrong:    return {100, 200, 255, 255};
    case TropoLevel::Extreme:       return {255, 100, 255, 255};
  }
  return {255, 255, 255, 255};
}

void TropoPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void TropoPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  cat->drawText(renderer, "VHF Tropo", x_ + 10, y_ + 5, themes.accent, FontStyle::MicroBold);

  TropoData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid) {
    cat->drawText(renderer, "Waiting for data...", x_ + width_/2, y_ + height_/2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  int curY = y_ + 25;
  int pad = 10;

  // Current Level
  cat->drawText(renderer, "Current:", x_ + pad, curY, themes.text, FontStyle::Fast);
  SDL_Color levelCol = getLevelColor(d.currentLevel);
  cat->drawText(renderer, tropoLevelToString(d.currentLevel), x_ + width_ - pad, curY, levelCol, FontStyle::Fast, false, true);
  
  curY += 20;

  // Simple Bar
  int barW = width_ - 2 * pad;
  int barH = 10;
  SDL_Rect barFrame = {x_ + pad, curY, barW, barH};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &barFrame);
  
  int fillW = (static_cast<int>(d.currentLevel) * barW) / 8;
  SDL_Rect barFill = {x_ + pad, curY, fillW, barH};
  SDL_SetRenderDrawColor(renderer, levelCol.r, levelCol.g, levelCol.b, 255);
  SDL_RenderFillRect(renderer, &barFill);
  
  curY += 20;

  // Forecast
  cat->drawText(renderer, "Forecast:", x_ + pad, curY, themes.text, FontStyle::MicroBold);
  curY += 15;

  int i = 0;
  for (const auto& f : d.forecast) {
    if (curY + 15 > y_ + height_) break;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "+%dh:", f.hourOffset);
    cat->drawText(renderer, buf, x_ + pad, curY, themes.textDim, FontStyle::Micro);
    
    SDL_Color fCol = getLevelColor(f.level);
    cat->drawText(renderer, tropoLevelToString(f.level), x_ + width_ - pad, curY, fCol, FontStyle::Micro, false, true);
    
    curY += 14;
    if (++i >= 4) break;
  }
}

} // namespace HamClock

REGISTER_WIDGET("tropo", "Tropo", false, false, {
  return std::make_unique<HamClock::TropoPanel>(0, 0, 0, 0, deps.fontMgr);
})
