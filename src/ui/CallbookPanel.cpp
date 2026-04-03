#include "CallbookPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

CallbookPanel::CallbookPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<CallbookStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void CallbookPanel::update() { currentData_ = store_->get(); }

void CallbookPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, "Callbook");

  auto *cat = fontMgr_.catalog();
  if (!currentData_.valid) {
    cat->drawText(renderer, "NO CALLSIGN DATA", x_ + width_ / 2,
                  y_ + height_ / 2, themes.textDim, FontStyle::Micro, true);
    return;
  }

  int titleH = 20;
  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  // Callsign & Name
  cat->drawText(renderer, currentData_.callsign, centerX, curY, themes.accent,
                FontStyle::MediumBold, true);
  curY += cat->ptSize(FontStyle::MediumBold) + 4;

  cat->drawText(renderer, currentData_.name, centerX, curY, themes.text,
                FontStyle::SmallRegular, true);
  curY += cat->ptSize(FontStyle::SmallRegular) + 15;

  // Location / Grid
  char locBuf[64];
  std::snprintf(locBuf, sizeof(locBuf), "%s, %s", currentData_.city.c_str(),
                currentData_.country.empty() ? "USA"
                                             : currentData_.country.c_str());
  cat->drawText(renderer, locBuf, centerX, curY, themes.text, FontStyle::Micro,
                true);
  curY += cat->ptSize(FontStyle::Micro) + 4;

  std::snprintf(locBuf, sizeof(locBuf), "Grid: %s", currentData_.grid.c_str());
  cat->drawText(renderer, locBuf, centerX, curY, themes.success,
                FontStyle::MicroBold, true);
  curY += cat->ptSize(FontStyle::MicroBold) + 15;

  // QSL Info (Badges)
  int badgeX = x_ + 20;
  if (currentData_.lotw) {
    cat->drawText(renderer, "[LoTW]", badgeX, curY, themes.info,
                  FontStyle::Tiny);
    badgeX += 60;
  }
  if (currentData_.eqsl) {
    cat->drawText(renderer, "[eQSL]", badgeX, curY, themes.success,
                  FontStyle::Tiny);
  }

  // Attribution
  cat->drawText(renderer, currentData_.source, x_ + width_ - 5,
                y_ + height_ - 15, themes.textDim, FontStyle::Caption, false,
                true);
}

void CallbookPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

#include "WidgetRegistry.h"
REGISTER_WIDGET("callbook", "Callbook", false, false, {
  return std::make_unique<CallbookPanel>(0, 0, 0, 0, deps.fontMgr, deps.callbookStore);
})
