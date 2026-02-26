#include "AuroraGraphPanel.h"
#include "../core/Theme.h"

#include "RenderUtils.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

AuroraGraphPanel::AuroraGraphPanel(int x, int y, int w, int h,
                                   FontManager &fontMgr, TextureManager &texMgr,
                                   std::shared_ptr<AuroraHistoryStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      store_(std::move(store)) {}

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

  int titleH = 20;
  fontMgr_.drawText(renderer, "Aurora Chances", x_ + 10, y_ + 5, themes.accent,
                    10, true);

  if (!store_ || !store_->hasData()) {
    fontMgr_.drawText(renderer, "Loading Aurora...", x_ + width_ / 2,
                      y_ + titleH + (height_ - titleH) / 2,
                      {150, 150, 150, 255}, 12, false, true);
    return;
  }

  // Get history data
  auto history = store_->getHistory();
  float currentPercent = store_->getCurrentPercent();

  // Display current value prominently
  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%.0f", currentPercent);

  int valueFontSize = std::max(24, height_ / 3);
  fontMgr_.drawText(renderer, valueText, x_ + width_ / 2,
                    y_ + titleH + (height_ - titleH) / 4, {200, 200, 200, 255},
                    valueFontSize, false, true);

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

    // Don't draw line across large data gaps (> 1 hour)
    if (prevAge - currAge > 1.0f)
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

    SDL_Texture *lineAA = texMgr_.get("line_aa");
    if (lineAA) {
      RenderUtils::drawThickLineTextured(renderer, lineAA, (float)x1, (float)y1,
                                         (float)x2, (float)y2, 2.0f,
                                         {0, 255, 128, 255});
    } else {
      RenderUtils::drawThickLine(renderer, (float)x1, (float)y1, (float)x2,
                                 (float)y2, 2.0f, {0, 255, 128, 255});
    }
  }

  if (tooltip_.visible) {
    renderTooltip(renderer);
  }
}

void AuroraGraphPanel::onMouseMove(int mx, int my) {
  if (!store_ || !store_->hasData()) {
    tooltip_.visible = false;
    return;
  }

  // Graph area (must match render() logic)
  int graphX = x_ + 30;
  int graphY = y_ + height_ / 2;
  int graphW = width_ - 40;
  int graphH = height_ / 2 - 30;

  if (mx < graphX || mx > graphX + graphW || my < graphY ||
      my > graphY + graphH) {
    tooltip_.visible = false;
    return;
  }

  auto history = store_->getHistory();
  if (history.size() < 2) {
    tooltip_.visible = false;
    return;
  }

  auto now = std::chrono::system_clock::now();

  // Find nearest data point based on X coordinate
  float bestDist = 99999.0f;
  const AuroraDataPoint *bestPoint = nullptr;

  for (const auto &p : history) {
    auto age =
        std::chrono::duration_cast<std::chrono::minutes>(now - p.timestamp)
            .count() /
        60.0f;

    if (age > 24.0f)
      continue;

    int px = graphX + graphW - static_cast<int>(age * graphW / 24.0f);
    float dist = std::abs(static_cast<float>(mx - px));
    if (dist < bestDist) {
      bestDist = dist;
      bestPoint = &p;
    }
  }

  if (bestPoint && bestDist < 15.0f) {
    auto ageMins = std::chrono::duration_cast<std::chrono::minutes>(
                       now - bestPoint->timestamp)
                       .count();
    char buf[64];
    if (ageMins < 30) {
      std::snprintf(buf, sizeof(buf), "%.0f%% (Now)", bestPoint->percent);
    } else {
      std::snprintf(buf, sizeof(buf), "%.0f%% (-%lldh %lldm)",
                    bestPoint->percent, static_cast<long long>(ageMins / 60),
                    static_cast<long long>(ageMins % 60));
    }
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

void AuroraGraphPanel::renderTooltip(SDL_Renderer *renderer) {
  if (tooltip_.text.empty())
    return;

  int ptSize = 10;
  int tw = fontMgr_.getLogicalWidth(tooltip_.text, ptSize);
  int th = fontMgr_.getLogicalHeight(tooltip_.text, ptSize);

  int padX = 8;
  int padY = 4;
  int boxW = tw + padX * 2;
  int boxH = th + padY * 2;

  int bx = tooltip_.x - boxW / 2;
  int by = tooltip_.y - boxH - 12;

  // Flip if too close to top
  if (by < y_) {
    by = tooltip_.y + 16;
  }
  // Clamp to widget bounds
  if (bx < x_)
    bx = x_;
  if (bx + boxW > x_ + width_)
    bx = x_ + width_ - boxW;

  SDL_Rect box = {bx, by, boxW, boxH};
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 200);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
  SDL_RenderDrawRect(renderer, &box);

  fontMgr_.drawText(renderer, tooltip_.text, bx + padX, by + padY,
                    {255, 255, 255, 255}, ptSize);
}
