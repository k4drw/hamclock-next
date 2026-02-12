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
    std::snprintf(buf, sizeof(buf), "SFI:%d  K:%d  A:%d  SSN:%d", data.sfi,
                  data.k_index, data.a_index, data.sunspot_number);
    currentText_ = buf;
  }
}

void SolarPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

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

  bool isNarrow = (width_ < 100);
  SolarData data = store_->get();

  if (isNarrow) {
    // Vertical stack (Fidelity Mode style)
    int rowH = height_ / 3;
    int centerX = x_ + width_ / 2;

    auto drawSolarRow = [&](const char *lbl, int val, int rowIdx) {
      int ry = y_ + rowIdx * rowH;
      // Value (Large, Green)
      char valBuf[16];
      std::snprintf(valBuf, sizeof(valBuf), "%d", val);
      fontMgr_.drawText(renderer, valBuf, centerX, ry + rowH * 0.35f,
                        {0, 255, 0, 255}, valueFontSize_, true, true);
      // Label (Small)
      fontMgr_.drawText(renderer, lbl, centerX, ry + rowH * 0.75f,
                        themes.textDim, labelFontSize_, false, true);
    };

    if (data.valid) {
      drawSolarRow("Solar SFI", data.sfi, 0);
      drawSolarRow("Sunspots", data.sunspot_number, 1);

      // Mixed A/K row
      int ry = y_ + 2 * rowH;
      char akBuf[32];
      std::snprintf(akBuf, sizeof(akBuf), "A%d K%d", data.a_index,
                    data.k_index);
      fontMgr_.drawText(renderer, akBuf, centerX, ry + rowH * 0.35f,
                        {0, 255, 0, 255}, valueFontSize_, true, true);
      fontMgr_.drawText(renderer, "A & K", centerX, ry + rowH * 0.75f,
                        themes.textDim, labelFontSize_, false, true);
    }
    return;
  }

  // Wide Layout
  bool needRedraw = (currentText_ != lastText_) || (fontSize_ != lastFontSize_);
  if (needRedraw) {
    destroyCache();
    cached_ = fontMgr_.renderText(renderer, currentText_, themes.accent,
                                  fontSize_, &texW_, &texH_);
    lastText_ = currentText_;
    lastFontSize_ = fontSize_;
  }

  if (cached_) {
    // Left-aligned with 2% padding, vertically centered
    int drawX = x_ + static_cast<int>(width_ * 0.02f);
    int drawY = y_ + (height_ - texH_) / 2;
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
