#include "ForecastPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

ForecastPanel::ForecastPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<ForecastStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void ForecastPanel::update() {
  currentData_ = store_->get();
  useMetricTemp_ = useMetric_;
}

void ForecastPanel::render(SDL_Renderer *renderer) {
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

  int titleH = 20;
  int pad = 6;
  int curY = y_ + titleH + 4;
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "7-Day Forecast", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int centerX = x_ + width_ / 2;
  if (!currentData_.valid) {
    cat->drawText(renderer, "Loading...", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  if (currentData_.periods.empty()) {
    cat->drawText(renderer, "No forecast data", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  // Each row: period name + temp + short forecast
  int rowH = nameFontSize_ + detailFontSize_ + 4;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  maxScroll_ = std::max(0, (int)currentData_.periods.size() - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);
  int startIdx = std::max(0, scrollOffset_);
  int endIdx = std::min((int)currentData_.periods.size(), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &p = currentData_.periods[i];

    // Period name
    SDL_Color nameCol = p.isDaytime ? themes.text : themes.textDim;
    std::string nameTrunc = p.name;
    if (nameTrunc.size() > 14)
      nameTrunc = nameTrunc.substr(0, 13) + "~";
    cat->drawText(renderer, nameTrunc, x_ + pad, curY, nameCol, FontStyle::Fast);

    // Temperature (right-aligned)
    char tempBuf[16];
    if (useMetricTemp_) {
      int tempC = (int)((p.tempF - 32) * 5 / 9);
      std::snprintf(tempBuf, sizeof(tempBuf), "%d C", tempC);
    } else {
      std::snprintf(tempBuf, sizeof(tempBuf), "%d F", p.tempF);
    }
    SDL_Color tempCol = {0, 200, 255, 255};
    cat->drawText(renderer, tempBuf, x_ + width_ - pad, curY, tempCol,
                  FontStyle::Fast, false, true);

    // Short forecast below name
    std::string sf = p.shortForecast;
    if (sf.size() > 24)
      sf = sf.substr(0, 23) + "~";
    cat->drawText(renderer, sf, x_ + pad, curY + nameFontSize_ + 1,
                  themes.textDim, FontStyle::Micro);

    // Precip if non-zero
    if (p.precipitationPercent > 0) {
      char ppBuf[16];
      std::snprintf(ppBuf, sizeof(ppBuf), "%d%%", p.precipitationPercent);
      cat->drawText(renderer, ppBuf, x_ + width_ - pad,
                    curY + nameFontSize_ + 1, {100, 160, 255, 255},
                    FontStyle::Micro, false, true);
    }

    curY += rowH;

    // Horizontal divider between periods
    if (i < endIdx - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, 100);
      SDL_RenderDrawLine(renderer, x_ + pad, curY - 1, x_ + width_ - pad,
                         curY - 1);
    }
  }

  // Scroll indicator
  if ((int)currentData_.periods.size() > maxRows) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)currentData_.periods.size());
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - detailFontSize_, themes.textDim,
                  FontStyle::Tiny, false, true);
  }
}

void ForecastPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  nameFontSize_ = cat->ptSize(FontStyle::Fast);
  detailFontSize_ = cat->ptSize(FontStyle::Micro);
  scrollOffset_ = 0;
}

bool ForecastPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}
