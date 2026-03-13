#include "RepeaterPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

RepeaterPanel::RepeaterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<RepeaterStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void RepeaterPanel::update() { currentData_ = store_->get(); }

void RepeaterPanel::render(SDL_Renderer *renderer) {
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

  auto *cat = fontMgr_.catalog();
  int titleH = 20;
  int pad = 6;
  int curY = y_ + titleH + 4;

  cat->drawText(renderer, "Repeaters", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int centerX = x_ + width_ / 2;
  if (!currentData_.valid) {
    const char *msg = currentData_.fetched
                          ? "API key required" // 401 from RepeaterBook
                          : "Loading...";
    cat->drawText(renderer, msg, centerX, y_ + height_ / 2, themes.textDim,
                  FontStyle::Fast, true);
    if (currentData_.fetched)
      cat->drawText(renderer, "repeaterbook.com", centerX,
                    y_ + height_ / 2 + rowFontSize_ + 4, themes.textDim,
                    FontStyle::Micro, true);
    return;
  }

  if (currentData_.repeaters.empty()) {
    cat->drawText(renderer, "No repeaters found", centerX, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  // Each row: freq | callsign | distance (km or mi)
  int rowH = rowFontSize_ + subFontSize_ + 4;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  maxScroll_ = std::max(0, (int)currentData_.repeaters.size() - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);
  int startIdx = std::max(0, scrollOffset_);
  int endIdx = std::min((int)currentData_.repeaters.size(), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &r = currentData_.repeaters[i];

    // Frequency (left)
    char freqBuf[24];
    std::snprintf(freqBuf, sizeof(freqBuf), "%.4g", r.freqMHz);
    cat->drawText(renderer, freqBuf, x_ + pad, curY, themes.success,
                  FontStyle::Fast);

    // Callsign (center)
    cat->drawText(renderer, r.callsign, centerX, curY, themes.text,
                  FontStyle::Fast, true);

    // Distance (right)
    char distBuf[24];
    if (useMetric_)
      std::snprintf(distBuf, sizeof(distBuf), "%.0fkm", r.distanceKm);
    else
      std::snprintf(distBuf, sizeof(distBuf), "%.0fmi",
                    r.distanceKm * 0.621371);
    cat->drawText(renderer, distBuf, x_ + width_ - pad, curY, themes.textDim,
                  FontStyle::Fast, false, true);

    // Tone + mode (sub-row)
    char subBuf[48];
    std::snprintf(subBuf, sizeof(subBuf), "%s %s",
                  r.tone.empty() ? "no tone" : r.tone.c_str(),
                  r.mode.empty() ? "" : r.mode.c_str());
    cat->drawText(renderer, subBuf, x_ + pad, curY + rowFontSize_ + 1,
                  themes.textDim, FontStyle::Micro);

    curY += rowH;

    if (i < endIdx - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, 80);
      SDL_RenderDrawLine(renderer, x_ + pad, curY - 1, x_ + width_ - pad,
                         curY - 1);
    }
  }

  // Scroll indicator
  if ((int)currentData_.repeaters.size() > maxRows) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)currentData_.repeaters.size());
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - subFontSize_, themes.textDim,
                  FontStyle::Micro, false, true);
  }
}

void RepeaterPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  subFontSize_ = cat->ptSize(FontStyle::Micro);
  scrollOffset_ = 0;
}

bool RepeaterPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}
