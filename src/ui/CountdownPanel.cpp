#include "CountdownPanel.h"
#include "../core/Astronomy.h"

#include <cstdio>
#include <ctime>

CountdownPanel::CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {

  // Field Day 2026: June 27, 18:00 UTC
  struct tm t = {0};
  t.tm_year = 2026 - 1900;
  t.tm_mon = 5; // June
  t.tm_mday = 27;
  t.tm_hour = 18;
  targetTime_ =
      std::chrono::system_clock::from_time_t(Astronomy::portable_timegm(&t));
}

void CountdownPanel::update() {}

void CountdownPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  auto now = std::chrono::system_clock::now();
  auto diff =
      std::chrono::duration_cast<std::chrono::seconds>(targetTime_ - now)
          .count();

  int centerY = y_ + height_ / 2;
  int centerX = x_ + width_ / 2;

  fontMgr_.drawText(renderer, label_, centerX, y_ + 15, {0, 200, 255, 255}, 12,
                    true, true);

  if (diff <= 0) {
    fontMgr_.drawText(renderer, "EVENT ACTIVE!", centerX, centerY,
                      {255, 0, 0, 255}, 16, true, true);
    return;
  }

  int days = diff / 86400;
  int hours = (diff % 86400) / 3600;
  int mins = (diff % 3600) / 60;
  int secs = diff % 60;

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%dd %02dh %02dm %02ds", days, hours, mins,
                secs);
  fontMgr_.drawText(renderer, buf, centerX, centerY, {255, 255, 255, 255}, 18,
                    true, true);

  fontMgr_.drawText(renderer, "Remaining", centerX, y_ + height_ - 15,
                    {150, 150, 150, 255}, 10, false, true);
}
