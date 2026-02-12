#include "ClockAuxPanel.h"
#include "FontCatalog.h"
#include <chrono>
#include <cstdio>
#include <ctime>

ClockAuxPanel::ClockAuxPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void ClockAuxPanel::update() {}

void ClockAuxPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Background
  SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  auto now = std::chrono::system_clock::now();
  std::time_t now_c = std::chrono::system_clock::to_time_t(now);
  struct tm *gmt = std::gmtime(&now_c);

  int centerX = x_ + width_ / 2;
  int curY = y_ + 10;

  // Header
  fontMgr_.drawText(renderer, "UTC Time", centerX, curY, {0, 200, 255, 255},
                    labelFontSize_, true, true);
  curY += labelFontSize_ + 8;

  // Time
  char buf[64];
  std::strftime(buf, sizeof(buf), "%H:%M:%S", gmt);
  fontMgr_.drawText(renderer, buf, centerX, curY + timeFontSize_ / 2,
                    {255, 255, 255, 255}, timeFontSize_, true, true);
  curY += timeFontSize_ + 12;

  // Date
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", gmt);
  fontMgr_.drawText(renderer, buf, centerX, curY, {200, 200, 200, 255},
                    infoFontSize_, false, true);
  curY += infoFontSize_ + 8;

  // Extra Info: Day of Year and Julian Date (simplified approximation)
  int doy = gmt->tm_yday + 1;
  // JD = Days since Jan 1, 4713 BC. Simplified: days since base point.
  // JD for 2000-01-01 12:00 is 2451545.0
  double jd = (double)now_c / 86400.0 + 2440587.5;

  std::snprintf(buf, sizeof(buf), "DOY %03d", doy);
  fontMgr_.drawText(renderer, buf, centerX, curY, {150, 150, 150, 255},
                    infoFontSize_, false, true);
  curY += infoFontSize_ + 6;

  std::snprintf(buf, sizeof(buf), "JD %.2f", jd);
  fontMgr_.drawText(renderer, buf, centerX, curY, {120, 120, 120, 255},
                    infoFontSize_, false, true);
}

void ClockAuxPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  timeFontSize_ = cat->ptSize(FontStyle::SmallBold);
  infoFontSize_ = cat->ptSize(FontStyle::Fast);

  if (h > 120) {
    timeFontSize_ = cat->ptSize(FontStyle::SmallBold) * 1.5; // Slight boost
  }
}
