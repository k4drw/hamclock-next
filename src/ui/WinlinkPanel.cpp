#include "WinlinkPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

WinlinkPanel::WinlinkPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<WinlinkStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void WinlinkPanel::update() { currentData_ = store_->get(); }

void WinlinkPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  int titleH = 20;
  int pad = 6;
  int curY = y_ + titleH + 4;
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Winlink RMS", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int centerX = x_ + width_ / 2;
  if (!currentData_.valid) {
    const char *msg =
        currentData_.fetched ? "Service unavailable" : "Loading...";
    cat->drawText(renderer, msg, centerX, y_ + height_ / 2, themes.textDim,
                  FontStyle::Fast, true);
    return;
  }

  if (currentData_.gateways.empty()) {
    cat->drawText(renderer, "No gateways found", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  // Each row: callsign | distance | freq | modes
  int rowH = rowFontSize_ + subFontSize_ + 4;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  maxScroll_ = std::max(0, (int)currentData_.gateways.size() - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);
  int startIdx = std::max(0, scrollOffset_);
  int endIdx = std::min((int)currentData_.gateways.size(), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &gw = currentData_.gateways[i];

    // Callsign
    cat->drawText(renderer, gw.callsign, x_ + pad, curY, themes.text,
                  FontStyle::Fast);

    // Distance (right)
    char distBuf[24];
    if (useMetric_)
      std::snprintf(distBuf, sizeof(distBuf), "%.0fkm", gw.distanceKm);
    else
      std::snprintf(distBuf, sizeof(distBuf), "%.0fmi",
                    gw.distanceKm * 0.621371);
    cat->drawText(renderer, distBuf, x_ + width_ - pad, curY, themes.textDim,
                  FontStyle::Fast, false, true);

    curY += rowFontSize_ + 1;

    // Freq + modes sub-row
    char subBuf[48];
    if (gw.freqMHz > 0)
      std::snprintf(subBuf, sizeof(subBuf), "%.4g MHz  %s", gw.freqMHz,
                    gw.modes.c_str());
    else
      std::snprintf(subBuf, sizeof(subBuf), "%s",
                    gw.modes.empty() ? "?" : gw.modes.c_str());
    cat->drawText(renderer, subBuf, x_ + pad, curY, themes.textDim,
                  FontStyle::Micro);

    curY += subFontSize_ + 3;

    if (i < endIdx - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, 80);
      SDL_RenderDrawLine(renderer, x_ + pad, curY - 1, x_ + width_ - pad,
                         curY - 1);
    }
  }

  if ((int)currentData_.gateways.size() > maxRows) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)currentData_.gateways.size());
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - subFontSize_, themes.textDim,
                  FontStyle::Micro, false, true);
  }
}

void WinlinkPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  subFontSize_ = cat->ptSize(FontStyle::Micro);
  scrollOffset_ = 0;
}

bool WinlinkPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}

REGISTER_WIDGET("winlink", "Winlink", false, true, {
  return std::make_unique<WinlinkPanel>(0, 0, 0, 0, deps.fontMgr, deps.winlinkStore);
})
