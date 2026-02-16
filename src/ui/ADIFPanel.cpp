#include "ADIFPanel.h"
#include "../core/Theme.h"
#include <algorithm>
#include <cstdio>
#include <vector>

ADIFPanel::ADIFPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<ADIFStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

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

  int pad = 8;
  int curY = y_ + pad;

  fontMgr_.drawText(renderer, "ADIF Log Stats", x_ + pad, curY, themes.accent,
                    10, true);
  curY += 16;

  if (!stats_.valid) {
    fontMgr_.drawText(renderer, "No Log Found", x_ + width_ / 2,
                      y_ + height_ / 2, themes.textDim, 12, false, true);
    return;
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "Total QSOs: %d", stats_.totalQSOs);
  fontMgr_.drawText(renderer, buf, x_ + pad, curY, themes.text, 11);
  curY += 18;

  // Top Bands
  std::vector<std::pair<std::string, int>> topBands(stats_.bandCounts.begin(),
                                                    stats_.bandCounts.end());
  std::sort(topBands.begin(), topBands.end(),
            [](auto &a, auto &b) { return a.second > b.second; });

  fontMgr_.drawText(renderer, "Top Bands:", x_ + pad, curY, themes.textDim, 9);
  curY += 12;
  for (size_t i = 0; i < std::min((size_t)3, topBands.size()); ++i) {
    std::snprintf(buf, sizeof(buf), "%s: %d", topBands[i].first.c_str(),
                  topBands[i].second);
    fontMgr_.drawText(renderer, buf, x_ + pad + 5, curY, themes.text, 10);
    curY += 12;
  }
  curY += 5;

  // Latest Calls
  fontMgr_.drawText(renderer, "Latest:", x_ + pad, curY, themes.textDim, 9);
  curY += 12;
  for (const auto &call : stats_.latestCalls) {
    fontMgr_.drawText(renderer, call, x_ + pad + 5, curY, themes.accent, 10);
    curY += 12;
  }
}

void ADIFPanel::renderLogView(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  int pad = 4;
  int headerY = y_ + pad;

  // Title
  fontMgr_.drawText(renderer, "Recent QSOs", x_ + pad, headerY, themes.accent,
                    10, true);
  headerY += headerHeight_;

  if (!stats_.valid || stats_.recentQSOs.empty()) {
    fontMgr_.drawText(renderer, "No QSOs Found", x_ + width_ / 2,
                      y_ + height_ / 2, themes.textDim, 12, false, true);
    return;
  }

  // Calculate scrolling
  int availableHeight = height_ - headerHeight_ - pad * 2;
  int visibleRows = availableHeight / rowHeight_;
  int totalRows = static_cast<int>(stats_.recentQSOs.size());
  maxScroll_ = std::max(0, totalRows - visibleRows);

  // Clamp scroll offset
  scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll_);

  // Column widths (percentages of available width)
  int scrollbarW = 8;
  int contentW = width_ - scrollbarW - pad * 2;

  // Column layout: Call(20%), Time(25%), Band(12%), Mode(12%), RST(10%), Grid(21%)
  int colCall = x_ + pad;
  int colTime = colCall + static_cast<int>(contentW * 0.20);
  int colBand = colTime + static_cast<int>(contentW * 0.25);
  int colMode = colBand + static_cast<int>(contentW * 0.12);
  int colRST = colMode + static_cast<int>(contentW * 0.12);
  int colGrid = colRST + static_cast<int>(contentW * 0.10);

  // Column headers
  SDL_Color headerColor = themes.textDim;
  fontMgr_.drawText(renderer, "Call", colCall, headerY, headerColor, 9, true);
  fontMgr_.drawText(renderer, "Time", colTime, headerY, headerColor, 9, true);
  fontMgr_.drawText(renderer, "Band", colBand, headerY, headerColor, 9, true);
  fontMgr_.drawText(renderer, "Mode", colMode, headerY, headerColor, 9, true);
  fontMgr_.drawText(renderer, "RST", colRST, headerY, headerColor, 9, true);
  fontMgr_.drawText(renderer, "Grid", colGrid, headerY, headerColor, 9, true);

  // Header separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, x_ + pad, headerY + 12,
                     x_ + width_ - scrollbarW - pad, headerY + 12);

  // Render visible QSO rows
  int rowY = headerY + 14;
  int endIdx = std::min(scrollOffset_ + visibleRows, totalRows);

  for (int i = scrollOffset_; i < endIdx; ++i) {
    const QSORecord &qso = stats_.recentQSOs[i];

    // Alternate row background
    if (i % 2 == 0) {
      SDL_SetRenderDrawColor(renderer, themes.bg.r + 10, themes.bg.g + 10,
                             themes.bg.b + 10, themes.bg.a);
      SDL_Rect rowRect = {x_ + pad, rowY - 1, contentW, rowHeight_};
      SDL_RenderFillRect(renderer, &rowRect);
    }

    SDL_Color textColor = themes.text;

    // Callsign (highlighted)
    fontMgr_.drawText(renderer, qso.callsign, colCall, rowY, themes.accent, 9);

    // Time
    std::string timeStr = formatTime(qso.date, qso.time);
    fontMgr_.drawText(renderer, timeStr, colTime, rowY, textColor, 9);

    // Band
    fontMgr_.drawText(renderer, qso.band, colBand, rowY, textColor, 9);

    // Mode
    fontMgr_.drawText(renderer, qso.mode, colMode, rowY, textColor, 9);

    // RST (sent/rcvd)
    std::string rstStr = qso.rstSent;
    if (!qso.rstRcvd.empty()) {
      rstStr += "/" + qso.rstRcvd;
    }
    fontMgr_.drawText(renderer, rstStr, colRST, rowY, textColor, 9);

    // Grid
    fontMgr_.drawText(renderer, qso.gridsquare, colGrid, rowY, textColor, 9);

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
    int thumbY = scrollbarY +
                 static_cast<int>((float)scrollOffset_ / maxScroll_ *
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

bool ADIFPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (draggingScrollbar_) {
    draggingScrollbar_ = false;
    return true;
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
