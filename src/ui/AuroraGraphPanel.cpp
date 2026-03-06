#include "AuroraGraphPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

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
  auto *cat = fontMgr_.catalog();

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
  cat->drawText(renderer, "Aurora Chances", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!store_ || !store_->hasData()) {
    cat->drawText(renderer, "Loading Aurora...", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, {150, 150, 150, 255},
                  FontStyle::Fast, true);
    return;
  }

  // Get history data
  auto history = store_->getHistory();
  float currentPercent = store_->getCurrentPercent();

  // Display current value prominently (top-right area)
  char valueText[32];
  std::snprintf(valueText, sizeof(valueText), "%.0f%%", currentPercent);

  cat->drawText(renderer, valueText, x_ + width_ - 35, y_ + 5,
                {200, 200, 200, 255}, FontStyle::SmallBold, true);

  // Graph area - significantly expanded
  int graphX = x_ + 30;
  int graphY = y_ + titleH + 10;
  int graphW = width_ - 40;
  int graphH = height_ - titleH - 40;

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
    cat->drawText(renderer, label, graphX - 20, gy, {0, 255, 128, 255},
                  FontStyle::Tiny, false, false, true);
  }

  // X-axis labels
  cat->drawText(renderer, "-25", graphX, graphY + graphH + 10,
                {0, 255, 128, 255}, FontStyle::Tiny, false, false, true);
  cat->drawText(renderer, "Hours", graphX + graphW / 2, graphY + graphH + 10,
                {0, 255, 128, 255}, FontStyle::Tiny, true, false, true);
  cat->drawText(renderer, "0", graphX + graphW - 10, graphY + graphH + 10,
                {0, 255, 128, 255}, FontStyle::Tiny, false, false, true);

  // Plot data
  if (history.size() < 2) {
    cat->drawText(renderer, "Collecting history...", x_ + width_ / 2,
                  y_ + (graphY + graphH / 2), {100, 100, 100, 255},
                  FontStyle::Micro, true);
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

    // Dim color for Kp-estimated (backfill) segments
    SDL_Color segColor = (prev.isBackfill || curr.isBackfill)
                             ? SDL_Color{0, 140, 80, 180}
                             : SDL_Color{0, 255, 128, 255};

    SDL_Texture *lineAA = texMgr_.get("line_aa");
    if (lineAA) {
      RenderUtils::drawThickLineTextured(renderer, lineAA, (float)x1, (float)y1,
                                         (float)x2, (float)y2, 2.0f, segColor);
    } else {
      RenderUtils::drawThickLine(renderer, (float)x1, (float)y1, (float)x2,
                                 (float)y2, 2.0f, segColor);
    }
  }

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
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
    const char *src = bestPoint->isBackfill ? " [Est.]" : "";
    if (ageMins < 30) {
      std::snprintf(buf, sizeof(buf), "%.0f%% (Now)%s", bestPoint->percent,
                    src);
    } else {
      std::snprintf(buf, sizeof(buf), "%.0f%% (-%lldh %lldm)%s",
                    bestPoint->percent, static_cast<long long>(ageMins / 60),
                    static_cast<long long>(ageMins % 60), src);
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

