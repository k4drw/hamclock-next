#include "IonosondePanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"

namespace HamClock {

IonosondePanel::IonosondePanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr) {}

void IonosondePanel::updateData(const IonosondeData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

void IonosondePanel::render(SDL_Renderer *renderer) {
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

  cat->drawText(renderer, "Live Ionosonde", x_ + 10, y_ + 5, themes.accent, FontStyle::MicroBold);

  IonosondeData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid || d.stations.empty()) {
    cat->drawText(renderer, "Waiting for DIDBase...", x_ + width_/2, y_ + height_/2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  int curY = y_ + 25;
  int pad = 10;

  // Overview stats
  cat->drawText(renderer, "Avg foF2:", x_ + pad, curY, themes.text, FontStyle::Fast);
  
  SDL_Color idxCol = {100, 200, 100, 255}; // Green
  if (d.avgFof2 < 5.0f) idxCol = {200, 200, 100, 255}; // Yellow
  if (d.avgFof2 < 3.0f) idxCol = {255, 100, 100, 255}; // Red
  
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f MHz", d.avgFof2);
  cat->drawText(renderer, buf, x_ + width_ - pad, curY, idxCol, FontStyle::Fast, false, true);
  
  curY += 22;
  
  // Highlight best station
  float maxF = 0;
  std::string bestName = "";
  for (const auto& st : d.stations) {
    if (st.foF2 > maxF) { maxF = (float)st.foF2; bestName = st.name; }
  }

  std::string s = "Peak: " + bestName + " (" + std::to_string(maxF).substr(0, 3) + ")";
  cat->drawText(renderer, s, x_ + pad, curY, themes.accent, FontStyle::Micro);
  
  curY += 20;

  // Graph
  cat->drawText(renderer, "Regional foF2 (MHz)", x_ + pad, curY, themes.textDim, FontStyle::MicroBold);
  drawGraph(renderer, x_ + pad, curY + 12, width_ - 2*pad, 35, d.stations);

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }
}

void IonosondePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

void IonosondePanel::onMouseMove(int mx, int my) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!data_.valid || data_.stations.empty()) {
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

  int count = (int)data_.stations.size();
  int idx = (mx - gx) * (count - 1) / gw;
  if (idx >= 0 && idx < count) {
    const auto& st = data_.stations[idx];
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s: %.1f MHz\nhmF2: %.0f km", 
                 st.name.c_str(), st.foF2, st.hmF2.value_or(0.0));
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

void IonosondePanel::drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const std::vector<IonosondeStation>& stations) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 150);
  SDL_Rect frame = {x, y, w, h};
  SDL_RenderDrawRect(renderer, &frame);

  if (stations.empty()) return;

  float maxVal = 10.0f; // Minimum scale up to 10 MHz
  for(const auto& st : stations) {
    if(st.foF2 > maxVal) maxVal = (float)st.foF2;
  }
  maxVal += 1.0f; // Add a bit of headroom

  SDL_Texture *lineTex = texMgr_.get("line_aa");
  std::vector<SDL_FPoint> pts;
  pts.reserve(stations.size());

  int count = (int)stations.size();
  for (int i = 0; i < count; ++i) {
    float px = x + (count > 1 ? (i * w) / (float)(count - 1) : w / 2.0f);
    float py = y + h - ((float)stations[i].foF2 / maxVal) * h;
    pts.push_back({px, py});
  }

  if (lineTex && count > 1) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, pts.data(), count, 2.0f, themes.accent);
  } else if (count > 1) {
    RenderUtils::drawPolyline(renderer, pts.data(), count, 1.5f, themes.accent);
  } else if (count == 1) {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, themes.accent.a);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x, (int)pts[0].y);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x+1, (int)pts[0].y);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x, (int)pts[0].y+1);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x+1, (int)pts[0].y+1);
  }
}


} // namespace HamClock
