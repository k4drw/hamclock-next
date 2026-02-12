#include "HistoryPanel.h"
#include "../core/Theme.h"
#include <SDL2/SDL.h>
#include <algorithm>

HistoryPanel::HistoryPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<HistoryStore> store,
                           const std::string &seriesName)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      seriesName_(seriesName) {}

void HistoryPanel::update() { currentSeries_ = store_->get(seriesName_); }

void HistoryPanel::render(SDL_Renderer *renderer) {
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

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int pad = 10;
  int graphW = width_ - 2 * pad;
  int graphH = height_ - 2 * pad - 12;
  int graphX = x_ + pad;
  int graphY = y_ + pad + 12;

  // Title
  std::string title = seriesName_ == "flux"
                          ? "Solar Flux"
                          : (seriesName_ == "ssn" ? "Sunspots" : "Planetary K");
  fontMgr_.drawText(renderer, title, x_ + pad, y_ + 5, themes.accent, 10, true);

  if (!currentSeries_.valid || currentSeries_.points.empty()) {
    fontMgr_.drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                      {100, 100, 100, 255}, 12, false, true);
    return;
  }

  // Axes
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  SDL_RenderDrawLine(renderer, graphX, graphY, graphX, graphY + graphH);
  SDL_RenderDrawLine(renderer, graphX, graphY + graphH, graphX + graphW,
                     graphY + graphH);

  float minV = currentSeries_.minValue;
  float maxV = currentSeries_.maxValue;
  if (maxV == minV)
    maxV += 1.0f;
  float range = maxV - minV;

  int n = static_cast<int>(currentSeries_.points.size());
  float stepX = (float)graphW / (std::max(1, n - 1));

  if (seriesName_ == "kp") {
    // Bar chart for Kp
    float barW = (float)graphW / 30.0f;
    for (int i = 0; i < n; ++i) {
      float val = currentSeries_.points[i].value;
      int bh = (int)((val / 9.0f) * graphH);
      SDL_Rect bar = {(int)(graphX + i * barW + 1), graphY + graphH - bh,
                      (int)barW - 1, bh};

      if (val >= 5)
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
      else if (val >= 4)
        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
      else
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

      SDL_RenderFillRect(renderer, &bar);
    }
  } else {
    // Line chart for Flux and SSN
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    for (int i = 0; i < n - 1; ++i) {
      float v1 = currentSeries_.points[i].value;
      float v2 = currentSeries_.points[i + 1].value;

      int y1 = graphY + graphH - (int)(((v1 - minV) / range) * graphH);
      int y2 = graphY + graphH - (int)(((v2 - minV) / range) * graphH);

      SDL_RenderDrawLine(renderer, (int)(graphX + i * stepX), y1,
                         (int)(graphX + (i + 1) * stepX), y2);
    }

    // Show current value
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f", currentSeries_.points.back().value);
    fontMgr_.drawText(renderer, buf, x_ + width_ - pad, y_ + 5,
                      {255, 255, 255, 255}, 10, true, true);
  }
}
