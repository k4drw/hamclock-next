#include "SolarPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cstdio>

void SolarPanel::update() {
  SolarData data = store_->get();
  if (!data.valid) {
    currentText_ = "Solar: awaiting data...";
  } else {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "SFI:%d  K:%.1f  A:%d  SSN:%d", data.sfi,
                  data.k_index, data.a_index, data.sunspot_number);
    currentText_ = buf;
  }
}

void SolarPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  bool isNarrow = (width_ < 100);
  SolarData data = store_->get();

  int titleH = 20;
  fontMgr_.catalog()->drawText(renderer, "Solar", x_ + 10, y_ + 5, themes.accent,
                               FontStyle::MicroBold);

  if (isNarrow) {
    // Vertical stack (Fidelity Mode style)
    int availableH = height_ - titleH;
    int rowH = availableH / 3;
    int centerX = x_ + width_ / 2;

    auto drawSolarRow = [&](const char *lbl, int val, int rowIdx) {
      int ry = y_ + titleH + rowIdx * rowH;
      // Value (Large, Green)
      char valBuf[16];
      std::snprintf(valBuf, sizeof(valBuf), "%d", val);
      fontMgr_.catalog()->drawText(renderer, valBuf, centerX, ry + rowH * 0.35f,
                                   themes.success, FontStyle::MediumBold,
                                   true);
      // Label (Small)
      fontMgr_.catalog()->drawText(renderer, lbl, centerX, ry + rowH * 0.75f,
                                   themes.textDim, FontStyle::Micro, true);
    };

    if (data.valid) {
      drawSolarRow("Solar SFI", data.sfi, 0);
      drawSolarRow("Sunspots", data.sunspot_number, 1);

      // Mixed A/K row
      int ry = y_ + titleH + 2 * rowH;
      char akBuf[32];
      std::snprintf(akBuf, sizeof(akBuf), "A%d K%.1f", data.a_index,
                    data.k_index);
      fontMgr_.catalog()->drawText(renderer, akBuf, centerX, ry + rowH * 0.35f,
                                   themes.success, FontStyle::MediumBold,
                                   true);
      fontMgr_.catalog()->drawText(renderer, "A & K", centerX, ry + rowH * 0.75f,
                                   themes.textDim, FontStyle::Micro, true);
    }
    return;
  }

  // Wide Layout
  bool needRedraw = (currentText_ != lastText_) || (fontSize_ != lastFontSize_) || !cached_;
  if (needRedraw) {
    destroyCache();
    // Slightly smaller font if title is present? Or just keep it.
    cached_ = fontMgr_.renderText(renderer, currentText_, themes.accent,
                                  fontSize_, &texW_, &texH_);
    lastText_ = currentText_;
    lastFontSize_ = fontSize_;
  }

  if (cached_) {
    // Left-aligned with 2% padding, vertically centered in remaining space
    int drawX = x_ + static_cast<int>(width_ * 0.02f);
    int drawY = y_ + titleH + (height_ - titleH - texH_) / 2;
    SDL_Rect dst = {drawX, drawY, texW_, texH_};
    SDL_RenderCopy(renderer, cached_, nullptr, &dst);
  }
}

void SolarPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (w < 100) {
    valueFontSize_ = cat->ptSize(FontStyle::MediumBold);
    labelFontSize_ = cat->ptSize(FontStyle::Micro);
  } else {
    fontSize_ = std::clamp(static_cast<int>(w * 0.05f), 8, 22);
  }
  destroyCache();
}
