#include "SantaPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

SantaPanel::SantaPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       std::shared_ptr<SantaStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void SantaPanel::update() { currentData_ = store_->get(); }

void SantaPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  renderChrome(renderer);

  int titleH = 20;
  cat->drawText(renderer, "Santa Tracker", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  if (!currentData_.active) {
    cat->drawText(renderer, "Resting at North Pole", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true);
    return;
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "Lat: %.1f", currentData_.lat);
  cat->drawText(renderer, buf, centerX, curY, themes.text, FontStyle::Micro,
                true);
  curY += 15;

  std::snprintf(buf, sizeof(buf), "Lon: %.1f", currentData_.lon);
  cat->drawText(renderer, buf, centerX, curY, themes.text, FontStyle::Micro,
                true);
  curY += 25;

  cat->drawText(renderer, "Status: Delivering!", centerX, curY,
                {0, 255, 100, 255}, FontStyle::MicroBold, true);
}
