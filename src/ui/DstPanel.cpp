#include "DstPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <SDL.h>
#include <cstdio>
#include <vector>

DstPanel::DstPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   TextureManager &texMgr, std::shared_ptr<DstStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), store_(store) {}

void DstPanel::update() { currentData_ = store_->get(); }

void DstPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
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

  cat->drawText(renderer, "Dst Index", x_ + pad, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!currentData_.valid || currentData_.points.empty()) {
    cat->drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                  {100, 100, 100, 255}, FontStyle::Micro, true);
    return;
  }

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

  // Baseline (Zero)
  int zeroY = graphY + graphH - (int)(((-minV) / range) * graphH);
  if (zeroY >= graphY && zeroY <= graphY + graphH) {
    RenderUtils::drawThickLine(renderer, (float)graphX, (float)zeroY,
                               (float)(graphX + graphW), (float)zeroY, 1.0f,
                               {80, 80, 80, 255});
  }

  // Plot
  int n = currentData_.points.size();
  for (int i = 0; i < n - 1; ++i) {
    const auto &p1 = currentData_.points[i];
    const auto &p2 = currentData_.points[i + 1];

    float x1 = (float)graphX + ((p1.age_hrs + 48.0f) / 48.0f * graphW);
    float x2 = (float)graphX + ((p2.age_hrs + 48.0f) / 48.0f * graphW);
    float y1 = (float)graphY + graphH - (((p1.value - minV) / range) * graphH);
    float y2 = (float)graphY + graphH - (((p2.value - minV) / range) * graphH);

    // Color based on value
    SDL_Color col;
    if (p2.value < -50)
      col = themes.danger; // Red for strong storm
    else if (p2.value < -20)
      col = themes.warning; // Yellow for moderate
    else
      col = {0, 255, 100, 255}; // Greenish-cyan for quiet

    SDL_Texture *lineAA = texMgr_.get("line_aa");
    if (lineAA) {
      RenderUtils::drawThickLineTextured(renderer, lineAA, x1, y1, x2, y2, 2.0f,
                                         col);
    } else {
      RenderUtils::drawThickLine(renderer, x1, y1, x2, y2, 2.0f, col);
    }
  }

  // Current value bubble
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%.0f nT", currentData_.current_val);
  cat->drawText(renderer, buf, x_ + width_ - pad, y_ + 5, {255, 255, 255, 255},
                FontStyle::Micro, false, true);

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

void DstPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
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


nlohmann::json DstPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  if (!currentData_.valid)
    return j;
  j["current_dst"] = currentData_.current_val;
  j["points_count"] = currentData_.points.size();
  return j;
}
