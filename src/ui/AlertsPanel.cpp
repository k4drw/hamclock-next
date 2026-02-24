#include "AlertsPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>
#include <cstdio>

AlertsPanel::AlertsPanel(int x, int y, int w, int h, FontManager &fontMgr,
                         std::shared_ptr<AlertsStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void AlertsPanel::update() { currentData_ = store_->get(); }

SDL_Color AlertsPanel::severityColor(const std::string &severity) {
  if (severity == "Extreme")
    return {255, 50, 50, 255};
  if (severity == "Severe")
    return {255, 140, 0, 255};
  if (severity == "Moderate")
    return {255, 220, 0, 255};
  if (severity == "Minor")
    return {180, 220, 180, 255};
  return {200, 200, 200, 255};
}

void AlertsPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int centerX = x_ + width_ / 2;
  int pad = 6;
  int curY = y_ + pad;

  fontMgr_.drawText(renderer, "WX Alerts", centerX, curY, themes.accent,
                    titleFontSize_, true, true);
  curY += titleFontSize_ + 6;

  if (!currentData_.valid) {
    fontMgr_.drawText(renderer, "Loading...", centerX, y_ + height_ / 2,
                      themes.textDim, rowFontSize_, false, true);
    return;
  }

  if (currentData_.alerts.empty()) {
    fontMgr_.drawText(renderer, "No active alerts", centerX, y_ + height_ / 2,
                      themes.success, rowFontSize_, false, true);
    return;
  }

  const auto &alerts = currentData_.alerts;
  int rowH = rowFontSize_ + 6;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  int startIdx = std::max(0, scrollOffset_);
  int endIdx = std::min((int)alerts.size(), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &a = alerts[i];
    SDL_Color col = severityColor(a.severity);

    // Severity indicator dot
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_Rect dot = {x_ + pad, curY + rowFontSize_ / 4, 5, rowFontSize_ / 2};
    SDL_RenderFillRect(renderer, &dot);

    // Event name (truncate if needed)
    std::string label = a.event;
    if (label.size() > 22)
      label = label.substr(0, 21) + "~";
    fontMgr_.drawText(renderer, label, x_ + pad + 10, curY, col, rowFontSize_,
                      true, false);
    curY += rowH;
  }

  // Scroll indicator when there are more alerts than fit
  if ((int)alerts.size() > maxRows) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  (int)alerts.size());
    fontMgr_.drawText(renderer, buf, x_ + width_ - pad,
                      y_ + height_ - pad - rowFontSize_, themes.textDim,
                      rowFontSize_ - 2, false, true);
  }
}

void AlertsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::FastBold);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  scrollOffset_ = 0;
}

bool AlertsPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::max(0, scrollOffset_ - scrollY);
  return true;
}
