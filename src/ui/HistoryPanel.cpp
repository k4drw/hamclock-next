#include "HistoryPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

HistoryPanel::HistoryPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           TextureManager &texMgr,
                           std::shared_ptr<HistoryStore> store,
                           const std::string &seriesName)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      store_(std::move(store)), seriesName_(seriesName) {}

void HistoryPanel::update() { currentSeries_ = store_->get(seriesName_); }

void HistoryPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

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
  int axisLabelH = 14; // reserve space for X-axis time labels
  int graphW = width_ - 2 * pad;
  int graphH = height_ - 2 * pad - 12 - axisLabelH;
  int graphX = x_ + pad;
  int graphY = y_ + pad + 12;

  // Title
  std::string title = seriesName_ == "flux"
                          ? "Solar Flux"
                          : (seriesName_ == "ssn" ? "Sunspots" : "Planetary K");
  cat->drawText(renderer, title, x_ + pad, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!currentSeries_.valid || currentSeries_.points.empty()) {
    cat->drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                  {100, 100, 100, 255}, FontStyle::Fast, true);
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
    float barW = (float)graphW / (float)n;
    for (int i = 0; i < n; ++i) {
      float val = std::max(0.0f, currentSeries_.points[i].value);
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

    // Kp Legend
    const char *labels[] = {"0", "4", "5", "9"};
    float vals[] = {0, 4, 5, 9};
    for (int i = 0; i < 4; ++i) {
      int ly = graphY + graphH - (int)((vals[i] / 9.0f) * graphH);
      cat->drawText(renderer, labels[i], graphX - 2, ly, themes.textDim,
                    FontStyle::Tiny, false, true);
    }

    // Current Kp value as large overlay
    {
      float kpNow = currentSeries_.points.back().value;
      if (kpNow >= 0.0f) {
        char kpBuf[8];
        std::snprintf(kpBuf, sizeof(kpBuf), "%.1f", kpNow);
        SDL_Color kpCol = (kpNow >= 5.0f)   ? themes.danger
                          : (kpNow >= 4.0f) ? themes.warning
                                            : themes.success;
        cat->drawText(renderer, kpBuf, graphX + graphW / 2, graphY + 15, kpCol,
                      FontStyle::MediumBold, true, false, true);
      }
    }
  } else {
    // Line chart for Flux and SSN (Anti-Aliased)
    SDL_Texture *lineTex = texMgr_.get("line_aa");
    std::vector<SDL_FPoint> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
      float val = currentSeries_.points[i].value;
      float py = graphY + graphH - ((val - minV) / range) * graphH;
      pts.push_back({graphX + i * stepX, py});
    }

    if (lineTex) {
      RenderUtils::drawPolylineTextured(renderer, lineTex, pts.data(), n, 2.0f,
                                        {255, 255, 0, 255});
    } else {
      RenderUtils::drawPolyline(renderer, pts.data(), n, 1.5f,
                                {255, 255, 0, 255});
    }

    // Show current value inside graph area (right edge, top of graph)
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f", currentSeries_.points.back().value);
    cat->drawText(renderer, buf, graphX + graphW - 20, graphY + 10,
                  {255, 255, 255, 255}, FontStyle::Fast, true);
  }

  // X-axis time labels at 4 evenly-spaced positions
  if (n >= 2) {
    auto now = std::chrono::system_clock::now();
    int timeY = graphY + graphH + 3;
    for (int i = 0; i <= 3; ++i) {
      int idx = (i * (n - 1)) / 3;
      int lx = graphX + static_cast<int>(idx * stepX);

      // Tick mark
      SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
      SDL_RenderDrawLine(renderer, lx, graphY + graphH, lx,
                         graphY + graphH + 3);

      // Label
      auto ageMins = std::chrono::duration_cast<std::chrono::minutes>(
                         now - currentSeries_.points[idx].time)
                         .count();
      std::string label;
      if (i == 3 || ageMins <= 30) {
        label = "Now";
      } else {
        long ageH = ageMins / 60;
        label = "-" + std::to_string(ageH) + "h";
      }
      cat->drawText(renderer, label, lx, timeY, themes.textDim, FontStyle::Tiny,
                    true);
    }
  }

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

void HistoryPanel::onMouseMove(int mx, int my) {
  if (!currentSeries_.valid || currentSeries_.points.empty()) {
    tooltip_.visible = false;
    return;
  }

  // Graph area (must match render() logic)
  int pad = 10;
  int axisLabelH = 14;
  int graphW = width_ - 2 * pad;
  int graphH = height_ - 2 * pad - 12 - axisLabelH;
  int graphX = x_ + pad;
  int graphY = y_ + pad + 12;

  if (mx < graphX || mx > graphX + graphW || my < graphY ||
      my > graphY + graphH) {
    tooltip_.visible = false;
    return;
  }

  int n = static_cast<int>(currentSeries_.points.size());
  auto now = std::chrono::system_clock::now();

  const HistoryPoint *bestPoint = nullptr;
  float bestDist = 99999.0f;

  if (seriesName_ == "kp") {
    // Bar chart
    float barW = (float)graphW / (float)n;
    int idx = (int)((mx - graphX) / barW);
    if (idx >= 0 && idx < n) {
      bestPoint = &currentSeries_.points[idx];
      bestDist = 0.0f;
    }
  } else {
    // Line chart
    float stepX = (float)graphW / (std::max(1, n - 1));
    int idx = (int)((mx - graphX + stepX / 2.0f) / stepX);
    if (idx >= 0 && idx < n) {
      bestPoint = &currentSeries_.points[idx];
      int px = graphX + (int)(idx * stepX);
      bestDist = std::abs(static_cast<float>(mx - px));
    }
  }

  if (bestPoint && bestDist < 15.0f) {
    auto ageMins =
        std::chrono::duration_cast<std::chrono::minutes>(now - bestPoint->time)
            .count();
    char buf[64];
    std::string unit =
        (seriesName_ == "kp") ? "" : (seriesName_ == "flux" ? " sfu" : "");
    char valStr[32];
    if (seriesName_ == "kp") {
      std::snprintf(valStr, sizeof(valStr), "%.1f", bestPoint->value);
    } else {
      std::snprintf(valStr, sizeof(valStr), "%.0f", bestPoint->value);
    }

    if (ageMins < 30) {
      std::snprintf(buf, sizeof(buf), "%s%s (Now)", valStr, unit.c_str());
    } else {
      std::snprintf(buf, sizeof(buf), "%s%s (-%lldh %lldm)", valStr,
                    unit.c_str(), static_cast<long long>(ageMins / 60),
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

