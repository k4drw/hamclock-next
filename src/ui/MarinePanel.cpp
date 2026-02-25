#include "MarinePanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

MarinePanel::MarinePanel(int x, int y, int w, int h, FontManager &fontMgr,
                         std::shared_ptr<MarineStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void MarinePanel::update() { currentData_ = store_->get(); }

void MarinePanel::render(SDL_Renderer *renderer) {
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
  int centerX = x_ + width_ / 2;
  int curY = y_ + titleH + 4;

  fontMgr_.drawText(renderer, "Marine", x_ + 10, y_ + 5, themes.accent, 10,
                    true);

  if (!currentData_.tidesValid && !currentData_.buoyValid) {
    fontMgr_.drawText(renderer, "No data configured", centerX, y_ + height_ / 2,
                      themes.textDim, rowFontSize_, false, true);
    return;
  }

  // Tide section
  if (currentData_.tidesValid && !currentData_.tides.empty()) {
    fontMgr_.drawText(renderer, "TIDES", x_ + pad, curY, themes.accent,
                      subFontSize_, true, false);
    curY += subFontSize_ + 2;

    for (size_t i = 0; i < currentData_.tides.size() && i < 4; ++i) {
      const auto &t = currentData_.tides[i];
      char buf[48];
      bool isHigh = (t.type == "H");
      float ht = useMetric_ ? (float)(t.heightFt * 0.3048) : (float)t.heightFt;
      std::snprintf(buf, sizeof(buf), "%s  %s  %.2f%s", t.time.c_str(),
                    isHigh ? "HI" : "LO", ht, useMetric_ ? "m" : "ft");
      SDL_Color col =
          isHigh ? SDL_Color{0, 200, 255, 255} : SDL_Color{180, 160, 100, 255};
      fontMgr_.drawText(renderer, buf, x_ + pad, curY, col, rowFontSize_, false,
                        false);
      curY += rowFontSize_ + 3;
    }
    curY += 4;
  }

  // Buoy section
  if (currentData_.buoyValid) {
    fontMgr_.drawText(renderer, "BUOY", x_ + pad, curY, themes.accent,
                      subFontSize_, true, false);
    curY += subFontSize_ + 2;

    const auto &b = currentData_.buoy;
    char buf[64];

    if (b.waveHeightM >= 0) {
      float wh =
          useMetric_ ? (float)b.waveHeightM : (float)(b.waveHeightM * 3.281);
      std::snprintf(buf, sizeof(buf), "Waves: %.1f%s @ %.0fs", wh,
                    useMetric_ ? "m" : "ft",
                    b.wavePeriodS > 0 ? b.wavePeriodS : 0.0);
      fontMgr_.drawText(renderer, buf, x_ + pad, curY, themes.text,
                        rowFontSize_, false, false);
      curY += rowFontSize_ + 2;
    }

    if (b.waterTempC > -900) {
      float wt =
          useMetric_ ? (float)b.waterTempC : (float)(b.waterTempC * 1.8 + 32);
      std::snprintf(buf, sizeof(buf), "Water: %.1f%s", wt,
                    useMetric_ ? "C" : "F");
      fontMgr_.drawText(renderer, buf, x_ + pad, curY, {0, 200, 200, 255},
                        rowFontSize_, false, false);
      curY += rowFontSize_ + 2;
    }

    if (b.windSpeedMps >= 0) {
      float ws = useMetric_ ? (float)b.windSpeedMps
                            : (float)(b.windSpeedMps * 1.944); // kts
      std::snprintf(buf, sizeof(buf), "Wind: %.0f%s @ %d deg", ws,
                    useMetric_ ? "m/s" : "kt", b.windDirDeg);
      fontMgr_.drawText(renderer, buf, x_ + pad, curY, themes.textDim,
                        rowFontSize_, false, false);
    }
  }
}

void MarinePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  subFontSize_ = cat->ptSize(FontStyle::Micro);
}
