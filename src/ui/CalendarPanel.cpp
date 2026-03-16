#include "CalendarPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <ctime>

CalendarPanel::CalendarPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<CalendarStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void CalendarPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  if (showSetup_) {
    renderSetup(renderer);
    return;
  }

  renderChrome(renderer);
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  cat->drawText(renderer, "Calendar", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!store_) {
    cat->drawText(renderer, "No data", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true);
    return;
  }

  auto events = store_->snapshot();
  time_t nowT = std::time(nullptr);
  time_t horizon = nowT + 24 * 3600;

  // Filter and sort: only events that end in the future and start within 24h
  std::vector<const CalendarEvent *> upcoming;
  for (const auto &ev : events) {
    if (ev.end >= nowT && ev.start <= horizon)
      upcoming.push_back(&ev);
  }
  std::sort(upcoming.begin(), upcoming.end(),
            [](const CalendarEvent *a, const CalendarEvent *b) {
              return a->start < b->start;
            });

  const int titleH = 20;
  const int footerH = 18;
  const int rowH = 28;
  const int pad = 6;
  int rowY = y_ + titleH + pad;
  int maxRows = (height_ - titleH - pad - footerH) / rowH;

  if (upcoming.empty()) {
    cat->drawText(renderer, "No upcoming events", x_ + width_ / 2,
                  y_ + height_ / 2 - footerH / 2, themes.textDim,
                  FontStyle::Caption, true);
  } else {
    for (int i = 0; i < (int)upcoming.size() && i < maxRows; ++i) {
      const CalendarEvent &ev = *upcoming[i];

      // Time label
      // - Already-started events (start in past, end in future): "Active"
      // - All-day event today: "All day"
      // - All-day event another day: "M/D" date
      // - Future timed event: "HH:MMZ"
      char timeBuf[16];
      bool alreadyStarted = (ev.start <= nowT && ev.end > nowT);
      if (alreadyStarted && !ev.allDay) {
        std::snprintf(timeBuf, sizeof(timeBuf), "Active");
      } else if (ev.allDay) {
        struct tm evDay{};
        struct tm todayTm{};
        Astronomy::portable_gmtime(&ev.start, &evDay);
        Astronomy::portable_gmtime(&nowT, &todayTm);
        bool isToday = (evDay.tm_year == todayTm.tm_year &&
                        evDay.tm_mon  == todayTm.tm_mon  &&
                        evDay.tm_mday == todayTm.tm_mday);
        if (isToday) {
          std::snprintf(timeBuf, sizeof(timeBuf), "All day");
        } else {
          std::snprintf(timeBuf, sizeof(timeBuf), "%d/%d",
                        evDay.tm_mon + 1, evDay.tm_mday);
        }
      } else {
        struct tm t{};
        Astronomy::portable_gmtime(&ev.start, &t);
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02dZ",
                      t.tm_hour, t.tm_min);
      }

      // Highlight row if event is happening now or imminent (< 15 min)
      bool active = alreadyStarted;
      bool soon = (!active && ev.start > nowT && ev.start - nowT < 15 * 60);
      SDL_Color rowCol = active  ? themes.accent
                       : soon    ? SDL_Color{255, 200, 0, 255}
                                 : themes.text;

      // Time column (fixed 52px)
      cat->drawText(renderer, timeBuf, x_ + pad, rowY + rowH / 2 - 6,
                    themes.textDim, FontStyle::Micro);

      // Summary (truncated to fit)
      std::string summary = ev.summary;
      if (summary.size() > 28)
        summary = summary.substr(0, 27) + "\xe2\x80\xa6"; // UTF-8 ellipsis

      cat->drawText(renderer, summary.c_str(), x_ + pad + 54,
                    rowY + rowH / 2 - 6, rowCol, FontStyle::Caption);

      // Divider
      if (i < (int)upcoming.size() - 1 && i < maxRows - 1) {
        SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                               themes.border.b, 60);
        SDL_RenderDrawLine(renderer, x_ + pad, rowY + rowH - 1,
                           x_ + width_ - pad, rowY + rowH - 1);
      }

      rowY += rowH;
    }
  }

  // Footer tap target — "Settings" centered at bottom (like LiveSpotPanel)
  int fy = y_ + height_ - footerH;
  footerRect_ = {x_, fy, width_, footerH};
  cat->drawText(renderer, "Settings", x_ + width_ / 2, fy + 2,
                themes.textDim, FontStyle::Micro, true);
}

void CalendarPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bg);

  int cx = x_ + width_ / 2;
  int y = y_ + 10;

  // Title
  cat->drawText(renderer, "--- Calendar Config ---", cx, y, themes.accent,
                FontStyle::MicroBold, true);
  y += 22;

  const int btnW = 22;
  const int btnH = 22;
  const int rowH = 32;
  int lx = x_ + 10;

  // Row 1: Alert (mins)
  {
    char label[32];
    std::snprintf(label, sizeof(label), "Alert: %d min", pendingMinutes_);
    cat->drawText(renderer, label, lx, y + 4, themes.text, FontStyle::Caption);

    int bx = x_ + width_ - btnW * 2 - 16;
    minsDecBtn_ = {bx, y, btnW, btnH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &minsDecBtn_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &minsDecBtn_);
    cat->drawText(renderer, "-", bx + btnW / 2, y + 3, themes.text,
                  FontStyle::MicroBold, true);

    bx = x_ + width_ - btnW - 6;
    minsIncBtn_ = {bx, y, btnW, btnH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &minsIncBtn_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &minsIncBtn_);
    cat->drawText(renderer, "+", bx + btnW / 2, y + 3, themes.text,
                  FontStyle::MicroBold, true);

    y += rowH;
  }

  // Row 2: All-day notify hour (UTC)
  {
    char label[32];
    std::snprintf(label, sizeof(label), "All-day: %02d:00Z", pendingHour_);
    cat->drawText(renderer, label, lx, y + 4, themes.text, FontStyle::Caption);

    int bx = x_ + width_ - btnW * 2 - 16;
    hourDecBtn_ = {bx, y, btnW, btnH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &hourDecBtn_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &hourDecBtn_);
    cat->drawText(renderer, "-", bx + btnW / 2, y + 3, themes.text,
                  FontStyle::MicroBold, true);

    bx = x_ + width_ - btnW - 6;
    hourIncBtn_ = {bx, y, btnW, btnH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &hourIncBtn_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &hourIncBtn_);
    cat->drawText(renderer, "+", bx + btnW / 2, y + 3, themes.text,
                  FontStyle::MicroBold, true);

    y += rowH;
  }

  // Done button at bottom
  int doneW = 80;
  int doneH = 28;
  doneBtn_ = {cx - doneW / 2, y_ + height_ - doneH - 6, doneW, doneH};
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &doneBtn_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &doneBtn_);
  cat->drawText(renderer, "Done", cx, y_ + height_ - doneH - 6 + doneH / 2 - 6,
                themes.bg, FontStyle::MicroBold, true);
}

bool CalendarPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  (void)mod; (void)clicks;

  if (showSetup_)
    return handleSetupClick(mx, my);

  // Footer tap → open config
  if (mx >= footerRect_.x && mx <= footerRect_.x + footerRect_.w &&
      my >= footerRect_.y && my <= footerRect_.y + footerRect_.h) {
    pendingMinutes_ = notifyMinutes_;
    pendingHour_ = allDayHour_;
    showSetup_ = true;
    return true;
  }

  return false;
}

bool CalendarPanel::handleSetupClick(int mx, int my) {
  if (mx >= minsDecBtn_.x && mx <= minsDecBtn_.x + minsDecBtn_.w &&
      my >= minsDecBtn_.y && my <= minsDecBtn_.y + minsDecBtn_.h) {
    if (pendingMinutes_ > 1)
      --pendingMinutes_;
    return true;
  }
  if (mx >= minsIncBtn_.x && mx <= minsIncBtn_.x + minsIncBtn_.w &&
      my >= minsIncBtn_.y && my <= minsIncBtn_.y + minsIncBtn_.h) {
    if (pendingMinutes_ < 120)
      ++pendingMinutes_;
    return true;
  }
  if (mx >= hourDecBtn_.x && mx <= hourDecBtn_.x + hourDecBtn_.w &&
      my >= hourDecBtn_.y && my <= hourDecBtn_.y + hourDecBtn_.h) {
    if (pendingHour_ > 0)
      --pendingHour_;
    return true;
  }
  if (mx >= hourIncBtn_.x && mx <= hourIncBtn_.x + hourIncBtn_.w &&
      my >= hourIncBtn_.y && my <= hourIncBtn_.y + hourIncBtn_.h) {
    if (pendingHour_ < 23)
      ++pendingHour_;
    return true;
  }
  if (mx >= doneBtn_.x && mx <= doneBtn_.x + doneBtn_.w &&
      my >= doneBtn_.y && my <= doneBtn_.y + doneBtn_.h) {
    notifyMinutes_ = pendingMinutes_;
    allDayHour_ = pendingHour_;
    if (onConfigChanged_)
      onConfigChanged_(notifyMinutes_, allDayHour_);
    showSetup_ = false;
    return true;
  }
  return true;
}
