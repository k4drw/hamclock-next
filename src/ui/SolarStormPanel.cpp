#include "SolarStormPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <algorithm>
#include <cmath>

namespace HamClock {

SolarStormPanel::SolarStormPanel(int x, int y, int w, int h,
                                 FontManager &fontMgr, TextureManager &texMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr) {}

void SolarStormPanel::updateData(const SolarStormData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

void SolarStormPanel::render(SDL_Renderer *renderer) {
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

  cat->drawText(renderer, "Solar Storm Ops", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  SolarStormData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid) {
    cat->drawText(renderer, "Connecting SWPC...", x_ + width_ / 2,
                  y_ + height_ / 2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  int curY = y_ + 30;
  int pad = 10;
  int badgeW = (width_ - 2 * pad) / 3;

  // Badges (R, S, G)
  drawBadge(renderer, x_ + pad, curY, "R", d.rScale);
  drawBadge(renderer, x_ + pad + badgeW, curY, "S", d.sScale);
  drawBadge(renderer, x_ + pad + 2 * badgeW, curY, "G", d.gScale);

  curY += 45;

  // Flare Class Label
  std::string sClass = "Last Flare: " + d.lastFlareClass;
  SDL_Color flareCol = (d.rScale >= SolarStormScale::Strong)
                           ? themes.danger
                           : themes.text;
  cat->drawText(renderer, sClass, x_ + pad, curY, flareCol, FontStyle::Micro);

  // Alert if CME impact predicted
  if (d.cmeImpactPredicted) {
    cat->drawText(renderer, "!! CME ALERT !!", x_ + width_ - pad, curY,
                  themes.danger, FontStyle::Micro, false, true);
  }

  curY += 15;

  // Flux Sparkline
  cat->drawText(renderer, "60m X-ray Flux (Log)", x_ + pad, curY,
                themes.textDim, FontStyle::Micro);
  drawSparkline(renderer, x_ + pad, curY + 12, width_ - 2 * pad, 30,
                d.fluxHistory);

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

void SolarStormPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void SolarStormPanel::onMouseMove(int mx, int my) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!data_.valid) {
    tooltip_.visible = false;
    return;
  }

  int pad = 10;
  int curY = y_ + 30 + 45 + 15; // Matches sparkline pos
  int gx = x_ + pad;
  int gy = curY + 12;
  int gw = width_ - 2 * pad;
  int gh = 30;

  if (mx < gx || mx > gx + gw || my < gy || my > gy + gh) {
    tooltip_.visible = false;
    return;
  }

  int idx = (mx - gx) * 59 / gw;
  if (idx >= 0 && idx < 60) {
    float flux = data_.fluxHistory[idx];
    char buf[64];
    // Convert flux to Scientific notation
    std::snprintf(buf, sizeof(buf), "T-%dm: %.1e W/m2", (59 - idx), flux);
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

void SolarStormPanel::drawBadge(SDL_Renderer *renderer, int x, int y,
                                const std::string &label,
                                SolarStormScale scale) {
  ThemeColors themes = getThemeColors(theme_);
  int bw = 32;
  int bh = 34;
  int centerX = x + (((width_ - 20) / 3) / 2); // Center within its column
  SDL_Rect r = {centerX - bw / 2, y, bw, bh};

  SDL_Color fill;
  if (scale == SolarStormScale::None)
    fill = themes.rowStripe1;
  else if (scale == SolarStormScale::Minor)
    fill = themes.warning;
  else if (scale == SolarStormScale::Moderate)
    fill = themes.accent;
  else
    fill = themes.danger;

  SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
  SDL_RenderFillRect(renderer, &r);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 120);
  SDL_RenderDrawRect(renderer, &r);

  auto *cat = fontMgr_.catalog();
  // Label R/S/G
  cat->drawText(renderer, label, r.x + bw / 2, r.y + 4, themes.bg,
                FontStyle::MicroBold, true);
  // Value 0-5
  cat->drawText(renderer, std::to_string((int)scale), r.x + bw / 2, r.y + 18,
                themes.bg, FontStyle::Fast, true);
}

void SolarStormPanel::drawSparkline(SDL_Renderer *renderer, int x, int y, int w,
                                    int h, const float *values) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 150);
  SDL_Rect frame = {x, y, w, h};
  SDL_RenderDrawRect(renderer, &frame);

  SDL_Texture *lineTex = texMgr_.get("line_aa");
  std::vector<SDL_FPoint> pts;
  pts.reserve(60);

  float logMin = -9.0f;
  float logMax = -2.5f;

  for (int i = 0; i < 60; ++i) {
    float flux = std::max(1e-10f, values[i]);
    float logVal = std::log10(flux);
    float norm = (logVal - logMin) / (logMax - logMin);
    norm = std::clamp(norm, 0.0f, 1.0f);

    float px = x + (i * w) / 59.0f;
    float py = y + h - (norm * h);
    pts.push_back({px, py});
  }

  if (lineTex) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, pts.data(), 60, 1.5f,
                                      themes.warning);
  } else {
    RenderUtils::drawPolyline(renderer, pts.data(), 60, 1.5f,
                              themes.warning);
  }
}


} // namespace HamClock
