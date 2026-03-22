#include "GreylineWindowsPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

GreylineWindowsPanel::GreylineWindowsPanel(int x, int y, int w, int h,
                                           FontManager &fontMgr,
                                           std::shared_ptr<HamClockState> state)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(std::move(state)) {}

void GreylineWindowsPanel::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  struct tm utc {};
  Astronomy::portable_gmtime(&t, &utc);
  int doy = utc.tm_yday + 1;
  currentUTC_ = utc.tm_hour + utc.tm_min / 60.0 + utc.tm_sec / 3600.0;

  // Read location — called on main thread, no lock needed
  deTimes_ = Astronomy::calculateSunTimes(state_->deLocation.lat,
                                          state_->deLocation.lon, doy);
  hasDX_ = state_->dxActive || !state_->dxCallsign.empty();
  if (hasDX_) {
    dxTimes_ = Astronomy::calculateSunTimes(state_->dxLocation.lat,
                                            state_->dxLocation.lon, doy);
    bearing_ =
        Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
    distKm_ =
        Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
  }
}

double GreylineWindowsPanel::hoursToNextEvent(double nowUTC,
                                               const SunTimes &times) {
  double best = 9999.0;
  if (times.hasRise) {
    double diff = times.sunrise - nowUTC;
    if (diff < 0.0)
      diff += 24.0;
    if (diff < best)
      best = diff;
  }
  if (times.hasSet) {
    double diff = times.sunset - nowUTC;
    if (diff < 0.0)
      diff += 24.0;
    if (diff < best)
      best = diff;
  }
  return best;
}

// Fill a colored band on the 24h bar for [startH, endH] UTC hours.
// Handles midnight wrap-around.
static void fillTimeWindow(SDL_Renderer *renderer, int bx, int by, int bw,
                           int bh, double startH, double endH, SDL_Color col) {
  // Normalize to [0, 24)
  while (startH < 0.0)
    startH += 24.0;
  while (startH >= 24.0)
    startH -= 24.0;
  while (endH < 0.0)
    endH += 24.0;
  while (endH >= 24.0)
    endH -= 24.0;

  int x0 = bx + static_cast<int>(startH / 24.0 * bw);
  int x1 = bx + static_cast<int>(endH / 24.0 * bw);

  SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
  if (x0 <= x1) {
    if (x1 > x0) {
      SDL_Rect r = {x0, by, x1 - x0, bh};
      SDL_RenderFillRect(renderer, &r);
    }
  } else {
    // Wraps past midnight
    SDL_Rect r1 = {bx, by, x1 - bx, bh};
    if (r1.w > 0)
      SDL_RenderFillRect(renderer, &r1);
    SDL_Rect r2 = {x0, by, bx + bw - x0, bh};
    if (r2.w > 0)
      SDL_RenderFillRect(renderer, &r2);
  }
}

void GreylineWindowsPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

  renderChrome(renderer);

  const int pad = 8;
  cat->drawText(renderer, "Grey-Line Windows", x_ + pad, y_ + 5,
                themes.accent, FontStyle::MicroBold);

  // 24h timeline bar
  int barX = x_ + pad;
  int barW = width_ - 2 * pad;
  int barY = y_ + 22;
  int barH = 12;

  // Background
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_Rect bg = {barX, barY, barW, barH};
  SDL_RenderFillRect(renderer, &bg);

  // DE windows (warning = yellow-ish)
  SDL_Color deCol = themes.warning;
  deCol.a = 220;
  if (deTimes_.hasRise)
    fillTimeWindow(renderer, barX, barY + 1, barW, barH - 2,
                   deTimes_.sunrise - kWindowHalfH,
                   deTimes_.sunrise + kWindowHalfH, deCol);
  if (deTimes_.hasSet)
    fillTimeWindow(renderer, barX, barY + 1, barW, barH - 2,
                   deTimes_.sunset - kWindowHalfH,
                   deTimes_.sunset + kWindowHalfH, deCol);

  // DX windows (info = blue-ish)
  if (hasDX_) {
    SDL_Color dxCol = themes.info;
    dxCol.a = 220;
    if (dxTimes_.hasRise)
      fillTimeWindow(renderer, barX, barY + 1, barW, barH - 2,
                     dxTimes_.sunrise - kWindowHalfH,
                     dxTimes_.sunrise + kWindowHalfH, dxCol);
    if (dxTimes_.hasSet)
      fillTimeWindow(renderer, barX, barY + 1, barW, barH - 2,
                     dxTimes_.sunset - kWindowHalfH,
                     dxTimes_.sunset + kWindowHalfH, dxCol);
  }

  // Bar border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bg);

  // Hour tick marks (0, 6, 12, 18, 24)
  for (int h = 0; h <= 24; h += 6) {
    int tx = barX + h * barW / 24;
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 180);
    SDL_RenderDrawLine(renderer, tx, barY + barH, tx, barY + barH + 3);
    char hbuf[4];
    std::snprintf(hbuf, sizeof(hbuf), "%d", h);
    cat->drawText(renderer, hbuf, tx, barY + barH + 4, themes.textDim,
                  FontStyle::Tiny,
                  (h > 0 && h < 24) /* centered */ ? true : false);
  }

  // Current time cursor
  int cursorX = barX + static_cast<int>(currentUTC_ / 24.0 * barW);
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderDrawLine(renderer, cursorX, barY - 2, cursorX, barY + barH + 2);

  // Legend and event times
  int curY = barY + barH + 16;

  // Legend dots
  SDL_SetRenderDrawColor(renderer, themes.warning.r, themes.warning.g,
                         themes.warning.b, 255);
  SDL_Rect deLeg = {barX, curY, 10, 7};
  SDL_RenderFillRect(renderer, &deLeg);
  cat->drawText(renderer, "DE", barX + 13, curY, themes.text, FontStyle::Tiny);

  if (hasDX_) {
    SDL_SetRenderDrawColor(renderer, themes.info.r, themes.info.g,
                           themes.info.b, 255);
    SDL_Rect dxLeg = {barX + 32, curY, 10, 7};
    SDL_RenderFillRect(renderer, &dxLeg);
    cat->drawText(renderer, "DX", barX + 45, curY, themes.text,
                  FontStyle::Tiny);
  }
  curY += 14;

  // DE times
  if (deTimes_.hasRise && deTimes_.hasSet) {
    int rh = static_cast<int>(deTimes_.sunrise);
    int rm = static_cast<int>((deTimes_.sunrise - rh) * 60.0);
    int sh = static_cast<int>(deTimes_.sunset);
    int sm = static_cast<int>((deTimes_.sunset - sh) * 60.0);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "DE  SR %02d:%02dZ  SS %02d:%02dZ", rh, rm,
                  sh, sm);
    cat->drawText(renderer, buf, barX, curY, themes.text, FontStyle::Tiny);
    curY += 12;
  } else {
    cat->drawText(renderer, "DE: polar day/night", barX, curY, themes.textDim,
                  FontStyle::Tiny);
    curY += 12;
  }

  // DX times + path
  if (hasDX_) {
    if (dxTimes_.hasRise && dxTimes_.hasSet) {
      int rh = static_cast<int>(dxTimes_.sunrise);
      int rm = static_cast<int>((dxTimes_.sunrise - rh) * 60.0);
      int sh = static_cast<int>(dxTimes_.sunset);
      int sm = static_cast<int>((dxTimes_.sunset - sh) * 60.0);
      char buf[48];
      std::snprintf(buf, sizeof(buf), "DX  SR %02d:%02dZ  SS %02d:%02dZ", rh,
                    rm, sh, sm);
      cat->drawText(renderer, buf, barX, curY, themes.text, FontStyle::Tiny);
      curY += 12;
    }
    // Short-path bearing/distance
    char pathBuf[48];
    if (useMetric_) {
      std::snprintf(pathBuf, sizeof(pathBuf), "SP %.0f\xc2\xb0 / %.0f km",
                    bearing_, distKm_);
    } else {
      std::snprintf(pathBuf, sizeof(pathBuf), "SP %.0f\xc2\xb0 / %.0f mi",
                    bearing_, distKm_ * 0.621371);
    }
    cat->drawText(renderer, pathBuf, barX, curY, themes.textDim,
                  FontStyle::Tiny);
    curY += 12;
  }

  // Next grey-line countdown
  double deNext = hoursToNextEvent(currentUTC_, deTimes_);
  double dxNext = hasDX_ ? hoursToNextEvent(currentUTC_, dxTimes_) : 9999.0;
  double nextH = std::min(deNext, dxNext);

  if (nextH < 9998.0) {
    int nh = static_cast<int>(nextH);
    int nm = static_cast<int>((nextH - nh) * 60.0);
    char nextBuf[32];
    std::snprintf(nextBuf, sizeof(nextBuf), "Next: %dh %02dm", nh, nm);
    cat->drawText(renderer, nextBuf, x_ + width_ - pad,
                  y_ + height_ - pad - 12, themes.accent, FontStyle::FastBold,
                  false, true);
  }
}

void GreylineWindowsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}
