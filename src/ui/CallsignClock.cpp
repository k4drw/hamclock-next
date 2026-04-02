#include "CallsignClock.h"
#include "../core/Astronomy.h"
#include "../core/TimeUtils.h"
#include "FontCatalog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

void CallsignClock::destroyCache() {
  if (callTex_) {
    MemoryMonitor::getInstance().destroyTexture(callTex_);
  }
  if (timeTex_) {
    MemoryMonitor::getInstance().destroyTexture(timeTex_);
  }
  if (dateTex_) {
    MemoryMonitor::getInstance().destroyTexture(dateTex_);
  }
}

void CallsignClock::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  char buf[32];
  currentTime_ = TimeUtils::hms(utc.tm_hour, utc.tm_min, utc.tm_sec) + " UTC";

  std::snprintf(buf, sizeof(buf), "%s %02d %s %04d",
                (const char *[]){"Sun", "Mon", "Tue", "Wed", "Thu", "Fri",
                                 "Sat"}[utc.tm_wday],
                utc.tm_mday,
                (const char *[]){"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov",
                                 "Dec"}[utc.tm_mon],
                1900 + utc.tm_year);
  currentDate_ = buf;
}

void CallsignClock::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Row layout: callsign ~40%, time ~35%, date ~25% of height
  int callRowH = static_cast<int>(height_ * 0.40f);
  int timeRowH = static_cast<int>(height_ * 0.35f);
  int pad = static_cast<int>(width_ * 0.05f);

  // Callsign (large, colored)
  if (!callTex_) {
    callTex_ = cat->renderText(renderer, callsign_, themes.accent,
                               FontStyle::MediumBold, &callW_, &callH_);
  }
  if (callTex_) {
    int dy = y_ + (callRowH - callH_) / 2;
    SDL_Rect dst = {x_ + pad, dy, callW_, callH_};
    SDL_RenderCopy(renderer, callTex_, nullptr, &dst);
  }

  // Time
  if (currentTime_ != lastTime_ || !timeTex_) {
    if (timeTex_) {
      MemoryMonitor::getInstance().destroyTexture(timeTex_);
    }
    timeTex_ = cat->renderText(renderer, currentTime_, themes.text,
                               FontStyle::SmallBold, &timeW_, &timeH_);
    lastTime_ = currentTime_;
  }
  if (timeTex_) {
    int dy = y_ + callRowH + (timeRowH - timeH_) / 2;
    SDL_Rect dst = {x_ + pad, dy, timeW_, timeH_};
    SDL_RenderCopy(renderer, timeTex_, nullptr, &dst);
  }

  // Date
  if (currentDate_ != lastDate_ || !dateTex_) {
    if (dateTex_) {
      MemoryMonitor::getInstance().destroyTexture(dateTex_);
    }
    dateTex_ = cat->renderText(renderer, currentDate_, themes.textDim,
                               FontStyle::SmallRegular, &dateW_, &dateH_);
    lastDate_ = currentDate_;
  }
  if (dateTex_) {
    int dy = y_ + callRowH + timeRowH;
    SDL_Rect dst = {x_ + pad, dy, dateW_, dateH_};
    SDL_RenderCopy(renderer, dateTex_, nullptr, &dst);
  }
}

void CallsignClock::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  destroyCache();
}

#include "WidgetRegistry.h"
REGISTER_WIDGET("callsign_clock", "Callsign/Clock", false, false, {
  return std::make_unique<CallsignClock>(0, 0, 0, 0, deps.fontMgr, deps.appCfg.callsign);
})
