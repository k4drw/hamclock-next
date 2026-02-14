#include "CallsignClock.h"
#include "../core/Astronomy.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

void CallsignClock::destroyCache() {
  if (callTex_) {
    SDL_DestroyTexture(callTex_);
    callTex_ = nullptr;
  }
  if (timeTex_) {
    SDL_DestroyTexture(timeTex_);
    timeTex_ = nullptr;
  }
  if (dateTex_) {
    SDL_DestroyTexture(dateTex_);
    dateTex_ = nullptr;
  }
}

void CallsignClock::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d UTC", utc.tm_hour, utc.tm_min,
                utc.tm_sec);
  currentTime_ = buf;

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

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Row layout: callsign ~40%, time ~35%, date ~25% of height
  int callRowH = static_cast<int>(height_ * 0.40f);
  int timeRowH = static_cast<int>(height_ * 0.35f);
  int pad = static_cast<int>(width_ * 0.02f);

  // Callsign (large, colored)
  if (callFontSize_ != lastCallFontSize_) {
    if (callTex_) {
      SDL_DestroyTexture(callTex_);
      callTex_ = nullptr;
    }
    SDL_Color orange = {255, 165, 0, 255};
    callTex_ = fontMgr_.renderText(renderer, callsign_, orange, callFontSize_,
                                   &callW_, &callH_);
    lastCallFontSize_ = callFontSize_;
  }
  if (callTex_) {
    int dy = y_ + (callRowH - callH_) / 2;
    SDL_Rect dst = {x_ + pad, dy, callW_, callH_};
    SDL_RenderCopy(renderer, callTex_, nullptr, &dst);
  }

  // Time
  if (currentTime_ != lastTime_ || timeFontSize_ != lastTimeFontSize_) {
    if (timeTex_) {
      SDL_DestroyTexture(timeTex_);
      timeTex_ = nullptr;
    }
    SDL_Color white = {255, 255, 255, 255};
    timeTex_ = fontMgr_.renderText(renderer, currentTime_, white, timeFontSize_,
                                   &timeW_, &timeH_);
    lastTime_ = currentTime_;
    lastTimeFontSize_ = timeFontSize_;
  }
  if (timeTex_) {
    int dy = y_ + callRowH + (timeRowH - timeH_) / 2;
    SDL_Rect dst = {x_ + pad, dy, timeW_, timeH_};
    SDL_RenderCopy(renderer, timeTex_, nullptr, &dst);
  }

  // Date
  if (currentDate_ != lastDate_ || dateFontSize_ != lastDateFontSize_) {
    if (dateTex_) {
      SDL_DestroyTexture(dateTex_);
      dateTex_ = nullptr;
    }
    SDL_Color cyan = {0, 200, 255, 255};
    dateTex_ = fontMgr_.renderText(renderer, currentDate_, cyan, dateFontSize_,
                                   &dateW_, &dateH_);
    lastDate_ = currentDate_;
    lastDateFontSize_ = dateFontSize_;
  }
  if (dateTex_) {
    int dy = y_ + callRowH + timeRowH;
    SDL_Rect dst = {x_ + pad, dy, dateW_, dateH_};
    SDL_RenderCopy(renderer, dateTex_, nullptr, &dst);
  }
}

void CallsignClock::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Height-based with caps to prevent giant text in taller top bar
  callFontSize_ = std::clamp(static_cast<int>(h * 0.15f), 12, 36);
  timeFontSize_ = std::clamp(static_cast<int>(h * 0.12f), 10, 28);
  dateFontSize_ = std::clamp(static_cast<int>(h * 0.08f), 8, 20);
  destroyCache();
}
