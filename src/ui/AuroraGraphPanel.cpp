#include "AuroraGraphPanel.h"
#include "../core/Theme.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

AuroraGraphPanel::AuroraGraphPanel(int x, int y, int w, int h,
                                   FontManager &fontMgr,
                                   std::shared_ptr<AuroraHistoryStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void AuroraGraphPanel::update() {
  // Data is updated by NOAAProvider
}

void AuroraGraphPanel::render(SDL_Renderer *renderer) {
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

  if (!store_ || !store_->hasData()) {
    fontMgr_.drawText(renderer, "Loading Aurora...", x_ + width_ / 2,
                      y_ + height_ / 2, {150, 150, 150, 255}, 12, false, true);
    return;
  }

  // Get history data
  auto history = store_->getHistory();
  float currentPercent = store_->getCurrentPercent();

  // Title
  fontMgr_.drawText(renderer, "Aurora Chances, max %", x_ + 5, y_ + 5,
                    {0, 255, 128, 255}, 10);

  // Display current value prominently
  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%.0f", currentPercent);

  int valueFontSize = std::max(24, height_ / 3);
  fontMgr_.drawText(renderer, valueText, x_ + width_ / 2, y_ + height_ / 4,
                    {200, 200, 200, 255}, valueFontSize, false, true);

  // Graph area
  int graphX = x_ + 30;
  int graphY = y_ + height_ / 2;
  int graphW = width_ - 40;
  int graphH = height_ / 2 - 30;

  if (graphW < 50 || graphH < 30)
    return; // Too small to draw graph

  // Draw grid lines and labels
  SDL_Color gridColor = {40, 40, 40, 255};

  // Horizontal grid lines (0, 20, 40, 60, 80, 100%)
  for (int pct = 0; pct <= 100; pct += 20) {
    int gy = graphY + graphH - (pct * graphH / 100);

    SDL_SetRenderDrawColor(renderer, gridColor.r, gridColor.g, gridColor.b,
                           gridColor.a);
    SDL_RenderDrawLine(renderer, graphX, gy, graphX + graphW, gy);

    // Y-axis labels
    char label[8];
    std::snprintf(label, sizeof(label), "%d", pct);
    fontMgr_.drawText(renderer, label, graphX - 20, gy - 4, {0, 255, 128, 255},
                      8);
  }

  // X-axis labels
  fontMgr_.drawText(renderer, "-25", graphX, graphY + graphH + 10,
                    {0, 255, 128, 255}, 8);
  fontMgr_.drawText(renderer, "Hours", graphX + graphW / 2,
                    graphY + graphH + 10, {0, 255, 128, 255}, 8, false, true);
  fontMgr_.drawText(renderer, "0", graphX + graphW - 10, graphY + graphH + 10,
                    {0, 255, 128, 255}, 8);

  // Plot data
  if (history.size() < 2) {
    fontMgr_.drawText(renderer, "Collecting history...", x_ + width_ / 2,
                      y_ + (graphY + graphH / 2), {100, 100, 100, 255}, 10,
                      false, true);
    return;
  }

  // Calculate time range (24 hours)
  auto now = std::chrono::system_clock::now();
  auto oldest = now - std::chrono::hours(24);

  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255); // Green line

  // Draw line graph
  for (size_t i = 1; i < history.size(); ++i) {
    const auto &prev = history[i - 1];
    const auto &curr = history[i];

    // Calculate time offset in hours from now
    auto prevAge =
        std::chrono::duration_cast<std::chrono::minutes>(now - prev.timestamp)
            .count() /
        60.0f;
    auto currAge =
        std::chrono::duration_cast<std::chrono::minutes>(now - curr.timestamp)
            .count() /
        60.0f;

    // Skip if too old
    if (prevAge > 24.0f)
      continue;

    // Map to graph coordinates
    // X: 0 hours (now) = right edge, -24 hours = left edge
    int x1 = graphX + graphW - static_cast<int>(prevAge * graphW / 24.0f);
    int x2 = graphX + graphW - static_cast<int>(currAge * graphW / 24.0f);

    // Y: 0% = bottom, 100% = top
    int y1 = graphY + graphH - static_cast<int>(prev.percent * graphH / 100.0f);
    int y2 = graphY + graphH - static_cast<int>(curr.percent * graphH / 100.0f);

    // Clamp to graph bounds
    x1 = std::max(graphX, std::min(graphX + graphW, x1));
    x2 = std::max(graphX, std::min(graphX + graphW, x2));
    y1 = std::max(graphY, std::min(graphY + graphH, y1));
    y2 = std::max(graphY, std::min(graphY + graphH, y2));

    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }
}
