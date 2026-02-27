#include "ADIFPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static const char *kFilterBands[] = {"All", "160m", "80m", "60m",  "40m",
                                     "30m", "20m",  "17m", "15m",  "12m",
                                     "10m", "6m",   "2m",  nullptr};
static const char *kFilterModes[] = {"All",  "CW", "SSB", "FT8",  "FT4",
                                     "RTTY", "AM", "FM",  nullptr};

ADIFPanel::ADIFPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {
  store_->setFilters(kFilterBands[filterBandIdx_],
                     kFilterModes[filterModeIdx_]);
}

void ADIFPanel::update() { stats_ = store_->get(); }

std::string ADIFPanel::formatTime(const std::string &date,
                                  const std::string &time) const {
  // Format: YYYYMMDD HHMMSS -> MM/DD HH:MM
  if (date.length() < 8 || time.length() < 4)
    return "";

  char buf[16];
  std::snprintf(buf, sizeof(buf), "%c%c/%c%c %c%c:%c%c", date[4], date[5],
                date[6], date[7], time[0], time[1], time[2], time[3]);
  return buf;
}

void ADIFPanel::renderStatsView(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  int pad = 8;
  int curY = y_ + pad;

  cat->drawText(renderer, "ADIF Log Stats", x_ + pad, curY, themes.accent,
                FontStyle::MicroBold);
  curY += 16;

  if (!stats_.valid) {
    cat->drawText(renderer, "No Log Found", x_ + width_ / 2, y_ + height_ / 2,
                  themes.info, FontStyle::Fast);
    return;
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "Total QSOs: %d", stats_.totalQSOs);
  cat->drawText(renderer, buf, x_ + pad, curY, themes.text, FontStyle::UI);
  curY += 18;

  // Top Bands
  std::vector<std::pair<std::string, int>> topBands(stats_.bandCounts.begin(),
                                                    stats_.bandCounts.end());
  std::sort(topBands.begin(), topBands.end(),
            [](auto &a, auto &b) { return a.second > b.second; });

  cat->drawText(renderer, "Top Bands:", x_ + pad, curY, themes.info,
                FontStyle::Caption);
  curY += 12;
  for (size_t i = 0; i < std::min((size_t)3, topBands.size()); ++i) {
    std::snprintf(buf, sizeof(buf), "%s: %d", topBands[i].first.c_str(),
                  topBands[i].second);
    cat->drawText(renderer, buf, x_ + pad + 5, curY, themes.text,
                  FontStyle::Micro);
    curY += 12;
  }
  curY += 5;

  // Latest Calls
  cat->drawText(renderer, "Latest:", x_ + pad, curY, themes.info,
                FontStyle::Caption);
  curY += 12;
  for (const auto &call : stats_.latestCalls) {
    cat->drawText(renderer, call, x_ + pad + 5, curY, themes.accent,
                  FontStyle::Micro);
    curY += 12;
  }
}

void ADIFPanel::renderLogView(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  int pad = 4;
  int headerY = y_ + pad;

  // Title with band/mode filter chips
  cat->drawText(renderer, "Recent QSOs", x_ + pad, headerY, themes.accent,
                FontStyle::MicroBold);
  // Filter chips (clickable): band on the right side of title row
  char chipBuf[32];
  std::snprintf(chipBuf, sizeof(chipBuf), "[%s %s]",
                kFilterBands[filterBandIdx_], kFilterModes[filterModeIdx_]);
  cat->drawText(renderer, chipBuf, x_ + width_ - 65, headerY, themes.info,
                FontStyle::Caption);
  headerY += headerHeight_;

  // Build filtered QSO list
  const char *bandFilter =
      (filterBandIdx_ == 0) ? nullptr : kFilterBands[filterBandIdx_];
  const char *modeFilter =
      (filterModeIdx_ == 0) ? nullptr : kFilterModes[filterModeIdx_];

  std::vector<const QSORecord *> filtered;
  for (const auto &qso : stats_.recentQSOs) {
    if (bandFilter && qso.band != bandFilter)
      continue;
    if (modeFilter && qso.mode != modeFilter)
      continue;
    filtered.push_back(&qso);
  }

  if (!stats_.valid || filtered.empty()) {
    cat->drawText(renderer, "No QSOs Found", x_ + width_ / 2, y_ + height_ / 2,
                  themes.info, FontStyle::Fast, true);
    return;
  }

  // Calculate scrolling
  int availableHeight = height_ - headerHeight_ - pad * 2;
  int visibleRows = availableHeight / rowHeight_;
  int totalRows = static_cast<int>(filtered.size());
  maxScroll_ = std::max(0, totalRows - visibleRows);

  // Clamp scroll offset
  scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll_);

  // Column widths (percentages of available width)
  int scrollbarW = 8;
  int contentW = width_ - scrollbarW - pad * 2;

  // Column layout: Call(20%), Time(25%), Band(12%), Mode(12%), RST(10%),
  // Grid(21%)
  int colCall = x_ + pad;
  int colTime = colCall + static_cast<int>(contentW * 0.20);
  int colBand = colTime + static_cast<int>(contentW * 0.25);
  int colMode = colBand + static_cast<int>(contentW * 0.12);
  int colRST = colMode + static_cast<int>(contentW * 0.12);
  int colGrid = colRST + static_cast<int>(contentW * 0.10);

  // Column headers
  SDL_Color headerColor = themes.info;
  cat->drawText(renderer, "Call", colCall, headerY, headerColor,
                FontStyle::Caption);
  cat->drawText(renderer, "Time", colTime, headerY, headerColor,
                FontStyle::Caption);
  cat->drawText(renderer, "Band", colBand, headerY, headerColor,
                FontStyle::Caption);
  cat->drawText(renderer, "Mode", colMode, headerY, headerColor,
                FontStyle::Caption);
  cat->drawText(renderer, "RST", colRST, headerY, headerColor,
                FontStyle::Caption);
  cat->drawText(renderer, "Grid", colGrid, headerY, headerColor,
                FontStyle::Caption);

  // Header separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, x_ + pad, headerY + 12,
                     x_ + width_ - scrollbarW - pad, headerY + 12);

  // Render visible QSO rows
  int rowY = headerY + 14;
  int endIdx = std::min(scrollOffset_ + visibleRows, totalRows);

  for (int i = scrollOffset_; i < endIdx; ++i) {
    const QSORecord &qso = *filtered[i];

    // Alternate row background
    if (i % 2 == 0) {
      SDL_SetRenderDrawColor(renderer, themes.bg.r + 10, themes.bg.g + 10,
                             themes.bg.b + 10, themes.bg.a);
      SDL_Rect rowRect = {x_ + pad, rowY - 1, contentW, rowHeight_};
      SDL_RenderFillRect(renderer, &rowRect);
    }

    SDL_Color textColor = themes.text;

    // Callsign (highlighted)
    cat->drawText(renderer, qso.callsign, colCall, rowY, themes.accent,
                  FontStyle::Caption);

    // Time
    std::string timeStr = formatTime(qso.date, qso.time);
    cat->drawText(renderer, timeStr, colTime, rowY, textColor,
                  FontStyle::Caption);

    // Band
    cat->drawText(renderer, qso.band, colBand, rowY, textColor,
                  FontStyle::Caption);

    // Mode
    cat->drawText(renderer, qso.mode, colMode, rowY, textColor,
                  FontStyle::Caption);

    // RST (sent/rcvd)
    std::string rstStr = qso.rstSent;
    if (!qso.rstRcvd.empty()) {
      rstStr += "/" + qso.rstRcvd;
    }
    cat->drawText(renderer, rstStr, colRST, rowY, textColor, FontStyle::Caption);

    // Grid
    cat->drawText(renderer, qso.gridsquare, colGrid, rowY, textColor,
                  FontStyle::Caption);

    rowY += rowHeight_;
  }

  // Scrollbar (if needed)
  if (maxScroll_ > 0) {
    int scrollbarX = x_ + width_ - scrollbarW - 2;
    int scrollbarY = y_ + headerHeight_ + pad * 2;
    int scrollbarH = availableHeight;

    // Scrollbar track
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, themes.border.a / 2);
    SDL_Rect trackRect = {scrollbarX, scrollbarY, scrollbarW, scrollbarH};
    SDL_RenderFillRect(renderer, &trackRect);

    // Scrollbar thumb
    float thumbRatio = (float)visibleRows / totalRows;
    int thumbH = std::max(20, static_cast<int>(scrollbarH * thumbRatio));
    int thumbY =
        scrollbarY + static_cast<int>((float)scrollOffset_ / maxScroll_ *
                                      (scrollbarH - thumbH));

    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 200);
    SDL_Rect thumbRect = {scrollbarX, thumbY, scrollbarW, thumbH};
    SDL_RenderFillRect(renderer, &thumbRect);
  }
}

void ADIFPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  // Render appropriate view
  if (showLogView_) {
    renderLogView(renderer);
  } else {
    renderStatsView(renderer);
  }
}

void ADIFPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  // Reset scroll on resize
  scrollOffset_ = 0;
}

bool ADIFPanel::onMouseWheel(int delta) {
  // Scroll by 3 rows per wheel notch
  scrollOffset_ -= delta * 3;
  scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll_);
  return true;
}

void ADIFPanel::onMouseMove(int mx, int my) {
  if (!draggingScrollbar_)
    return;

  int scrollbarH = height_ - headerHeight_ - 16;
  int visibleRows = scrollbarH / rowHeight_;
  int totalRows = static_cast<int>(stats_.recentQSOs.size());

  if (totalRows > visibleRows) {
    float thumbRatio = (float)visibleRows / totalRows;
    int thumbH = std::max(20, static_cast<int>(scrollbarH * thumbRatio));

    int deltaY = my - dragStartY_;
    int scrollDelta =
        static_cast<int>((float)deltaY / (scrollbarH - thumbH) * maxScroll_);

    scrollOffset_ = dragStartOffset_ + scrollDelta;
    scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll_);
  }
}

bool ADIFPanel::onMouseUp(int mx, int my, Uint16 /*mod*/, int clicks) {
  if (draggingScrollbar_) {
    draggingScrollbar_ = false;
    return true;
  }

  // Click in header row: check if in the filter box (chip)
  if (showLogView_ && my >= y_ && my < y_ + headerHeight_) {
    int chipX = x_ + width_ - 65;
    char chipBuf[32];
    std::snprintf(chipBuf, sizeof(chipBuf), "[%s %s]",
                  kFilterBands[filterBandIdx_], kFilterModes[filterModeIdx_]);
    int chipW = fontMgr_.getLogicalWidth(chipBuf, 9, true);

    if (mx >= chipX && mx < chipX + chipW) {
      if (mx < chipX + chipW / 2) {
        // Left half of chip cycles band
        int n = 0;
        while (kFilterBands[n])
          n++;
        filterBandIdx_ = (filterBandIdx_ + 1) % n;
      } else {
        // Right half of chip cycles mode
        int n = 0;
        while (kFilterModes[n])
          n++;
        filterModeIdx_ = (filterModeIdx_ + 1) % n;
      }
      scrollOffset_ = 0;
      store_->setFilters(kFilterBands[filterBandIdx_],
                         kFilterModes[filterModeIdx_]);
      return true;
    }
  }

  // Check if clicking on scrollbar to start drag
  if (showLogView_ && maxScroll_ > 0) {
    int scrollbarX = x_ + width_ - 8 - 2;
    int scrollbarY = y_ + headerHeight_ + 8;
    int scrollbarH = height_ - headerHeight_ - 16;

    if (mx >= scrollbarX && mx < scrollbarX + 8 && my >= scrollbarY &&
        my < scrollbarY + scrollbarH) {
      draggingScrollbar_ = true;
      dragStartY_ = my;
      dragStartOffset_ = scrollOffset_;
      return true;
    }
  }

  return false;
}
