#include "DEInfo.h"
#include "../core/Astronomy.h"
#include "../core/MemoryMonitor.h"
#include "FontCatalog.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>

DEInfo::DEInfo(int x, int y, int w, int h, FontManager &fontMgr,
               const std::string &callsign, const std::string &grid)
    : Widget(x, y, w, h), fontMgr_(fontMgr), callsign_(callsign), grid_(grid) {
  Astronomy::gridToLatLon(grid_, lat_, lon_);
}

void DEInfo::update() {
  lineText_[0] = "DE:";
  lineText_[1] = callsign_;

  // Local time from longitude
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);
  int utcOffset = static_cast<int>(lon_ / 15.0);
  int localHour = (utc.tm_hour + utcOffset + 24) % 24;

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%02d:%02d UTC%+d", localHour, utc.tm_min,
                utcOffset);
  lineText_[2] = buf;

  std::snprintf(buf, sizeof(buf), "%s %.1f%c %.1f%c", grid_.c_str(),
                std::fabs(lat_), lat_ >= 0 ? 'N' : 'S', std::fabs(lon_),
                lon_ >= 0 ? 'E' : 'W');
  lineText_[3] = buf;
}

void DEInfo::destroyCache() {
  for (int i = 0; i < kNumLines; ++i) {
    if (lineTex_[i]) {
      MemoryMonitor::getInstance().destroyTexture(lineTex_[i]);
    }
  }
}

void DEInfo::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Clip to widget bounds
  SDL_Rect clip = {x_, y_, width_, height_};
  SDL_RenderSetClipRect(renderer, &clip);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &clip);

  int pad = static_cast<int>(width_ * 0.04f);
  SDL_Color colors[kNumLines] = {
      {255, 165, 0, 255},   // "DE:" label: orange
      {255, 200, 0, 255},   // Callsign: yellow
      {255, 255, 255, 255}, // Local time: white
      {0, 255, 128, 255},   // Grid + lat/lon: green
  };

  int curY = y_ + pad;
  for (int i = 0; i < kNumLines; ++i) {
    bool needRedraw = !lineTex_[i] || (lineText_[i] != lastLineText_[i]) ||
                      (lineFontSize_[i] != lastLineFontSize_[i]);
    if (needRedraw) {
      if (lineTex_[i]) {
        MemoryMonitor::getInstance().destroyTexture(lineTex_[i]);
      }
      lineTex_[i] =
          fontMgr_.renderText(renderer, lineText_[i], colors[i],
                              lineFontSize_[i], &lineW_[i], &lineH_[i]);
      lastLineText_[i] = lineText_[i];
      lastLineFontSize_[i] = lineFontSize_[i];
    }
    if (lineTex_[i]) {
      SDL_Rect dst = {x_ + pad, curY, lineW_[i], lineH_[i]};
      SDL_RenderCopy(renderer, lineTex_[i], nullptr, &dst);
      curY += lineH_[i] + pad / 2;
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void DEInfo::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  // Use named font styles sized for the narrow 139px side column.
  // "DE:" and detail lines use Fast (~15px); callsign uses a mid-size
  // proportional to panel height for visual prominence.
  lineFontSize_[0] = cat->ptSize(FontStyle::Fast); // "DE:" label
  lineFontSize_[1] =
      std::clamp(h / 6, 8, cat->ptSize(FontStyle::SmallRegular)); // Callsign
  lineFontSize_[2] = cat->ptSize(FontStyle::Fast);                // Local time
  lineFontSize_[3] = cat->ptSize(FontStyle::Fast); // Grid + coords
  destroyCache();
}
