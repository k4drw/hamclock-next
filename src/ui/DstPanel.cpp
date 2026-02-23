#include "DstPanel.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <cstdio>

DstPanel::DstPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   std::shared_ptr<DstStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void DstPanel::update() { currentData_ = store_->get(); }

void DstPanel::render(SDL_Renderer *renderer) {
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

  int pad = 10;
  int graphW = width_ - 2 * pad;
  int graphH = height_ - 2 * pad - 12;
  int graphX = x_ + pad;
  int graphY = y_ + pad + 12;

  fontMgr_.drawText(renderer, "Dst Index", x_ + pad, y_ + 5, themes.accent, 10,
                    true);

  if (!currentData_.valid || currentData_.points.empty()) {
    fontMgr_.drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                      {100, 100, 100, 255}, 10, false, true);
    return;
  }

  // Baseline (Zero)
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  // Determine min/max for scaling
  float minV = 0, maxV = 0;
  for (const auto &p : currentData_.points) {
    if (p.value < minV)
      minV = p.value;
    if (p.value > maxV)
      maxV = p.value;
  }
  // Buffer for visibility
  minV -= 10;
  maxV += 10;
  float range = maxV - minV;

  int zeroY = graphY + graphH - (int)(((-minV) / range) * graphH);
  if (zeroY >= graphY && zeroY <= graphY + graphH) {
    SDL_RenderDrawLine(renderer, graphX, zeroY, graphX + graphW, zeroY);
  }

  // Plot
  int n = currentData_.points.size();
  for (int i = 0; i < n - 1; ++i) {
    const auto &p1 = currentData_.points[i];
    const auto &p2 = currentData_.points[i + 1];

    int x1 = graphX + (int)((p1.age_hrs + 48.0f) / 48.0f * graphW);
    int x2 = graphX + (int)((p2.age_hrs + 48.0f) / 48.0f * graphW);
    int y1 = graphY + graphH - (int)(((p1.value - minV) / range) * graphH);
    int y2 = graphY + graphH - (int)(((p2.value - minV) / range) * graphH);

    // Color based on value
    if (p2.value < -50)
      SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else if (p2.value < -20)
      SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    else
      SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);

    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
  }

  // Current value bubble
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%.0f nT", currentData_.current_val);
  fontMgr_.drawText(renderer, buf, x_ + width_ - pad, y_ + 5,
                    {255, 255, 255, 255}, 10, true, true);

  if (tooltip_.visible) {
    renderTooltip(renderer);
  }
}

void DstPanel::onMouseMove(int mx, int my) {
  if (!currentData_.valid || currentData_.points.empty()) {
    tooltip_.visible = false;
    return;
  }

  // Graph area (must match render() logic)
  int pad = 10;
  int graphW = width_ - 2 * pad;
  int graphH = height_ - 2 * pad - 12;
  int graphX = x_ + pad;
  int graphY = y_ + pad + 12;

  if (mx < graphX || mx > graphX + graphW || my < graphY ||
      my > graphY + graphH) {
    tooltip_.visible = false;
    return;
  }

  // Find nearest data point based on age_hrs
  // age_hrs is -48 to 0
  float bestDist = 99999.0f;
  const DstPoint *bestPoint = nullptr;

  for (const auto &p : currentData_.points) {
    int px = graphX + (int)((p.age_hrs + 48.0f) / 48.0f * graphW);
    float dist = std::abs(static_cast<float>(mx - px));
    if (dist < bestDist) {
      bestDist = dist;
      bestPoint = &p;
    }
  }

  if (bestPoint && bestDist < 10.0f) {
    char buf[64];
    int age = (int)std::abs(bestPoint->age_hrs);
    if (age == 0) {
      std::snprintf(buf, sizeof(buf), "%.0f nT (Now)", bestPoint->value);
    } else {
      std::snprintf(buf, sizeof(buf), "%.0f nT (-%dh)", bestPoint->value, age);
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

void DstPanel::renderTooltip(SDL_Renderer *renderer) {
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

  if (by < y_)
    by = tooltip_.y + 16;
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

nlohmann::json DstPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  if (!currentData_.valid)
    return j;
  j["current_dst"] = currentData_.current_val;
  j["points_count"] = currentData_.points.size();
  return j;
}
