#include "SantaPanel.h"
#include "../core/Theme.h"
#include <cstdio>

SantaPanel::SantaPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       std::shared_ptr<SantaStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void SantaPanel::update() { currentData_ = store_->get(); }

void SantaPanel::render(SDL_Renderer *renderer) {
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

  int titleH = 20;
  fontMgr_.drawText(renderer, "Santa Tracker", x_ + 10, y_ + 5, themes.accent,
                    10, true);

  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  if (!currentData_.active) {
    fontMgr_.drawText(renderer, "Resting at North Pole", centerX,
                      y_ + height_ / 2, themes.textDim, 10, false, true);
    return;
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "Lat: %.1f", currentData_.lat);
  fontMgr_.drawText(renderer, buf, centerX, curY, themes.text, 10, false, true);
  curY += 15;

  std::snprintf(buf, sizeof(buf), "Lon: %.1f", currentData_.lon);
  fontMgr_.drawText(renderer, buf, centerX, curY, themes.text, 10, false, true);
  curY += 25;

  fontMgr_.drawText(renderer, "Status: Delivering!", centerX, curY,
                    {0, 255, 100, 255}, 10, true, true);
}
