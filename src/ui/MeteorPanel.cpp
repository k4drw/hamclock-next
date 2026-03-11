#include "MeteorPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <algorithm>

namespace HamClock {

MeteorPanel::MeteorPanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr) {}

void MeteorPanel::updateData(const MeteorData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

void MeteorPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawBlendMode(renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  cat->drawText(renderer, "Meteor Scatter", x_ + 10, y_ + 5, themes.accent, FontStyle::MicroBold);

  MeteorData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid) {
    cat->drawText(renderer, "Calculating...", x_ + width_/2, y_ + height_/2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  int curY = y_ + 25;
  int pad = 10;

  // Activity Index
  cat->drawText(renderer, "Activity:", x_ + pad, curY, themes.text, FontStyle::Fast);
  
  SDL_Color idxCol = themes.success; // Green
  if (d.currentIndex > 4.0f) idxCol = themes.warning; // Yellow
  if (d.currentIndex > 7.0f) idxCol = themes.danger; // Red
  
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f / 10", d.currentIndex);
  cat->drawText(renderer, buf, x_ + width_ - pad, curY, idxCol, FontStyle::Fast, false, true);
  
  curY += 22;

  // Active Showers
  if (d.activeShowers.empty()) {
    cat->drawText(renderer, "Sporadic only", x_ + pad, curY, themes.textDim, FontStyle::Micro);
  } else {
    std::string s = "Shower: " + d.activeShowers[0];
    cat->drawText(renderer, s, x_ + pad, curY, themes.accent, FontStyle::Micro);
  }
  
  curY += 20;

  // Graph
  cat->drawText(renderer, "24h Potential", x_ + pad, curY, themes.textDim, FontStyle::MicroBold);
  drawGraph(renderer, x_ + pad, curY + 12, width_ - 2*pad, 35, d.hourlyActivity);

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

void MeteorPanel::onMouseMove(int mx, int my) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!data_.valid) {
    tooltip_.visible = false;
    return;
  }

  int pad = 10;
  int curY = y_ + 25 + 22 + 20; // Matches render() graph position
  int gx = x_ + pad;
  int gy = curY + 12;
  int gw = width_ - 2*pad;
  int gh = 35;

  if (mx < gx || mx > gx + gw || my < gy || my > gy + gh) {
    tooltip_.visible = false;
    return;
  }

  int idx = (mx - gx) * 23 / gw;
  if (idx >= 0 && idx < 24) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "+%dh: %.1f", idx, data_.hourlyActivity[idx]);
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

void MeteorPanel::drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const float* values) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 150);
  SDL_Rect frame = {x, y, w, h};
  SDL_RenderDrawRect(renderer, &frame);

  float maxVal = 0.1f;
  for(int i=0; i<24; ++i) if(values[i] > maxVal) maxVal = values[i];

  SDL_Texture *lineTex = texMgr_.get("line_aa");
  std::vector<SDL_FPoint> pts;
  pts.reserve(24);

  for (int i = 0; i < 24; ++i) {
    float px = x + (i * w) / 23.0f;
    float py = y + h - (values[i] / maxVal) * h;
    pts.push_back({px, py});
  }

  if (lineTex) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, pts.data(), 24, 2.0f, themes.info);
  } else {
    RenderUtils::drawPolyline(renderer, pts.data(), 24, 1.5f, themes.info);
  }
}


} // namespace HamClock
