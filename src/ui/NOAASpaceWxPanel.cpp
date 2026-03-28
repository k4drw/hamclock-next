#include "NOAASpaceWxPanel.h"
#include "FontCatalog.h"

#include <cstdio>

NOAASpaceWxPanel::NOAASpaceWxPanel(int x, int y, int w, int h,
                                   FontManager &fontMgr,
                                   std::shared_ptr<SolarDataStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

SDL_Color NOAASpaceWxPanel::colorForScale(int scale,
                                          const ThemeColors &themes) {
  if (scale <= 0)
    return themes.textDim;
  if (scale <= 2)
    return themes.success;
  if (scale <= 3)
    return themes.warning;
  return themes.danger;
}

void NOAASpaceWxPanel::update() {
  SolarData data = store_->get();
  dataValid_ = data.noaa_scales_valid;
  if (dataValid_) {
    for (int c = 0; c < 3; ++c)
      for (int d = 0; d < 4; ++d)
        scales_[c][d] = data.noaa_scales[c][d];
  }
}

void NOAASpaceWxPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // Title
  int titleH = std::max(14, height_ / 10);
  cat->drawText(renderer, "NOAA SpaceWx", x_ + 4, y_ + 3, themes.accent,
                FontStyle::CaptionBold);

  if (!dataValid_) {
    cat->drawText(renderer, "Awaiting data...", x_ + 4,
                  y_ + titleH + (height_ - titleH) / 2 - 6, themes.textDim,
                  FontStyle::Micro);
    return;
  }

  // Layout: leave top titleH for title, then divide remaining height into rows
  // Row 0: column headers (Now, D+1, D+2, D+3)
  // Rows 1-3: R, S, G
  int bodyY = y_ + titleH + 2;
  int bodyH = height_ - titleH - 4;
  int nRows = 4; // header + 3 data rows
  int rowH = bodyH / nRows;

  // Column layout: label column + 4 value columns
  int labelW = width_ / 3;
  int colW = (width_ - labelW) / 4;

  // Column headers
  static const char *colHeaders[4] = {"Now", "D+1", "D+2", "D+3"};
  for (int d = 0; d < 4; ++d) {
    int cx = x_ + labelW + d * colW + colW / 2;
    int cy = bodyY + rowH / 2;
    cat->drawText(renderer, colHeaders[d], cx, cy, themes.textDim,
                  FontStyle::Micro, true, false, true);
  }

  // Data rows: R, S, G
  static const char *rowLabels[3] = {"Radio", "Solar", "Geo"};
  static const char rowPrefixes[3] = {'R', 'S', 'G'};
  for (int c = 0; c < 3; ++c) {
    int rowY = bodyY + (c + 1) * rowH;

    // Row label
    cat->drawText(renderer, rowLabels[c], x_ + 4, rowY + rowH / 2,
                  themes.textDim, FontStyle::Micro, false, false, true);

    // Values
    for (int d = 0; d < 4; ++d) {
      int val = scales_[c][d];
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%c%d", rowPrefixes[c], val);
      int cx = x_ + labelW + d * colW + colW / 2;
      int cy = rowY + rowH / 2;
      SDL_Color col = colorForScale(val, themes);
      cat->drawText(renderer, buf, cx, cy, col, FontStyle::SmallRegular, true,
                    false, true);
    }
  }
}
