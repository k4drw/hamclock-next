#include "BeaconPanel.h"
#include "../core/BeaconData.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

BeaconPanel::BeaconPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void BeaconPanel::update() {
  active_ = provider_.getActiveBeacons();
  progress_ = provider_.getSlotProgress();
}

void BeaconPanel::render(SDL_Renderer *renderer) {
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

  if (isNarrow) {
    // Narrow Layout (Fidelity Mode style)
    int pad = 4;
    int centerX = x_ + width_ / 2;
    int curY = y_ + pad;

    // Title
    fontMgr_.drawText(renderer, "NCDXF", centerX, curY + labelFontSize_ / 2,
                      themes.text, labelFontSize_, false, true);
    curY += labelFontSize_ + 4;

    // Band colors (approx based on screenshot)
    SDL_Color bandColors[] = {
        {255, 255, 0, 255},   // 20m: Yellow
        {150, 255, 0, 255},   // 17m: Green
        {0, 255, 200, 255},   // 15m: Cyan
        {0, 150, 255, 255},   // 12m: Blue
        {255, 180, 200, 255}, // 10m: Pink
    };
    const char *freqs[] = {"14.10", "18.11", "21.15", "24.93", "28.20"};

    int availableH = (height_ - curY - 6);
    int rowH = availableH / 5;

    for (int i = 0; i < 5; ++i) {
      int ry = curY + i * rowH;
      int iconX = x_ + 10;
      int iconY = ry + rowH / 2;
      int triSize = 6; // Fixed small size like original

      // Draw Triangle (Pointing Up)
      SDL_Vertex verts[3];
      verts[0] = {
          {(float)iconX - triSize, (float)iconY + triSize}, bandColors[i], {0}};
      verts[1] = {
          {(float)iconX + triSize, (float)iconY + triSize}, bandColors[i], {0}};
      verts[2] = {{(float)iconX, (float)iconY - triSize}, bandColors[i], {0}};
      SDL_RenderGeometry(renderer, nullptr, verts, 3, nullptr, 0);

      // Frequency
      fontMgr_.drawText(renderer, freqs[i], x_ + 20, ry + rowH / 2,
                        bandColors[i], callfontSize_, false, false);

      // Progress bar if this band is being "displayed" (active for any beacon)
      // Actually, let's just draw the progress for the whole slot at the bottom
    }

    // Progress bar at the bottom
    int barH = 2;
    SDL_Rect progRect = {x_ + 2, y_ + height_ - barH - 2,
                         (int)((width_ - 4) * progress_), barH};
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &progRect);
    return;
  }

  // Original Wide Layout (if ever used)
  int pad = 4;
  int callWidth = (width_ > 150) ? 60 : 45;
  int bandWidth = (width_ - callWidth - 2 * pad) / 5;

  int rowHeight = (height_ - 2 * pad - labelFontSize_) / 18;
  if (rowHeight < 2)
    rowHeight = 2;

  // Headers
  int curX = x_ + pad + callWidth;
  const char *bands[] = {"20", "17", "15", "12", "10"};
  for (int i = 0; i < 5; ++i) {
    fontMgr_.drawText(renderer, bands[i], curX + bandWidth / 2, y_ + pad,
                      themes.textDim, labelFontSize_, false, true);
    curX += bandWidth;
  }

  // Rows
  int startY = y_ + pad + labelFontSize_ + 2;
  for (int i = 0; i < 18; ++i) {
    int rowY = startY + i * rowHeight;
    fontMgr_.drawText(renderer, NCDXF_BEACONS[i].callsign, x_ + pad, rowY,
                      themes.textDim, callfontSize_);

    for (const auto &a : active_) {
      if (a.index == i) {
        int cellX = x_ + pad + callWidth + a.bandIndex * bandWidth;
        SDL_Rect cell = {cellX + 2, rowY, bandWidth - 4, rowHeight - 1};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &cell);
      }
    }
  }

  // Progress bar
  int barH = 2;
  SDL_Rect progressRect = {x_ + pad, y_ + height_ - barH - 2,
                           (int)((width_ - 2 * pad) * progress_), barH};
  SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
  SDL_RenderFillRect(renderer, &progressRect);
}

void BeaconPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  callfontSize_ = cat->ptSize(FontStyle::MediumBold);

  if (w < 100) {
    labelFontSize_ = cat->ptSize(FontStyle::Micro); // "NCDXF"
    callfontSize_ =
        cat->ptSize(FontStyle::Micro); // Frequencies (12px fits ~24px row)
  } else if (h < 120) {
    labelFontSize_ = cat->ptSize(FontStyle::Micro);
    callfontSize_ = cat->ptSize(FontStyle::Micro);
  }
}
