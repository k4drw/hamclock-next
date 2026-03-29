#include "SpaceWeatherAlertsPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

SpaceWeatherAlertsPanel::SpaceWeatherAlertsPanel(
    int x, int y, int w, int h, FontManager &fontMgr,
    std::shared_ptr<SpaceWeatherAlertStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void SpaceWeatherAlertsPanel::update() { currentData_ = store_->get(); }

SDL_Color SpaceWeatherAlertsPanel::alertColor(SpaceWxAlertType type) const {
  ThemeColors themes = getThemeColors(theme_);
  switch (type) {
    case SpaceWxAlertType::SolarFlareX:
      return themes.danger;
    case SpaceWxAlertType::SolarFlareM:
      return themes.warning;
    case SpaceWxAlertType::GeoStorm:
      return themes.info;
    case SpaceWxAlertType::KIndex:
      return themes.accent;
    case SpaceWxAlertType::CMEArrival:
      return themes.success;
    case SpaceWxAlertType::ProtonEvent:
      return {255, 140, 200, 255};  // pink — no direct theme semantic
    default:
      return themes.textDim;
  }
}

void SpaceWeatherAlertsPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

  renderChrome(renderer);

  const int pad = 6;
  cat->drawText(renderer, "Space Wx Alerts", x_ + pad, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  int curY = y_ + 22;

  if (!currentData_.valid) {
    cat->drawText(renderer, "Loading...", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Fast, true, false, true);
    return;
  }

  if (currentData_.alerts.empty()) {
    cat->drawText(renderer, "No recent alerts", x_ + width_ / 2,
                  y_ + height_ / 2, themes.success, FontStyle::Fast, true,
                  false, true);
    return;
  }

  const bool wide = (width_ > 500);
  const auto &alerts = currentData_.alerts;
  // Each alert row: label line + summary line
  int rowH = rowFontSize_ + 14;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  maxScroll_ = std::max(0, static_cast<int>(alerts.size()) - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);

  // Time column: "MM-DD HH:MM" width + small gap
  int timeColW = fontMgr_.getLogicalWidth("00-00 00:00", rowFontSize_) + 4;

  int startIdx = scrollOffset_;
  int endIdx = std::min(static_cast<int>(alerts.size()), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &a = alerts[i];
    SDL_Color col = alertColor(a.type);

    // Color indicator dot
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_Rect dot = {x_ + pad, curY + 3, 6, 6};
    SDL_RenderFillRect(renderer, &dot);

    int textX = x_ + pad + 10;

    // Date/time first (left-aligned): "YYYY-MM-DD HH:MM" → "MM-DD HH:MM"
    if (!a.issueTime.empty()) {
      std::string t =
          a.issueTime.size() >= 16 ? a.issueTime.substr(5, 11) : a.issueTime;
      cat->drawText(renderer, t, textX, curY, themes.textDim, FontStyle::Tiny);
    }

    // Type label after the time column
    std::string lbl = a.typeLabel;
    for (const char *pfx : {"ALERT: ", "WARNING: ", "WATCH: ", "SUMMARY: "}) {
      if (lbl.find(pfx) == 0) {
        lbl = lbl.substr(strlen(pfx));
        break;
      }
    }
    if (!wide && lbl.size() > 22)
      lbl = lbl.substr(0, 21) + "~";
    cat->drawText(renderer, lbl, textX + timeColW, curY, col, FontStyle::Tiny);
    curY += rowFontSize_ + 2;

    // Summary / secondary line
    if (!a.summary.empty() && a.summary != a.typeLabel) {
      std::string summ = a.summary;
      for (const char *pfx : {"ALERT: ", "WARNING: ", "WATCH: ", "SUMMARY: "}) {
        if (summ.find(pfx) == 0) {
          summ = summ.substr(strlen(pfx));
          break;
        }
      }
      if (!wide && summ.size() > 50)
        summ = summ.substr(0, 49) + "~";
      cat->drawText(renderer, summ, textX, curY, themes.textDim,
                    FontStyle::Tiny);
    }
    curY += 12;
  }

  // Scroll indicator
  if (static_cast<int>(alerts.size()) > maxRows) {
    char buf[50];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  static_cast<int>(alerts.size()));
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - rowFontSize_, themes.textDim,
                  FontStyle::Caption, false, true);
  }

  if (tooltip_.visible)
    renderTooltip(renderer, fontMgr_);
}

void SpaceWeatherAlertsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  rowFontSize_ = cat ? cat->ptSize(FontStyle::Tiny) : 9;
  scrollOffset_ = 0;
}

bool SpaceWeatherAlertsPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}

void SpaceWeatherAlertsPanel::onMouseMove(int mx, int my) {
  tooltip_.visible = false;
  tooltip_.text.clear();
  tooltip_.text2.clear();

  if (!currentData_.valid || currentData_.alerts.empty())
    return;

  const int pad = 6;
  int curY = y_ + 22;
  int rowH = rowFontSize_ + 14;

  if (my < curY || my >= y_ + height_ - pad)
    return;

  int rowIdx = (my - curY) / rowH;
  int alertIdx = rowIdx + scrollOffset_;

  if (alertIdx < 0 || alertIdx >= static_cast<int>(currentData_.alerts.size()))
    return;

  const auto &a = currentData_.alerts[alertIdx];
  // Line 1: date/time; line 2: full untruncated alert label
  tooltip_.text = a.issueTime.size() >= 16 ? a.issueTime.substr(5, 11) : a.issueTime;
  tooltip_.text2 = a.typeLabel;
  tooltip_.x = mx;
  tooltip_.y = my;
  tooltip_.visible = true;
  tooltip_.timestamp = SDL_GetTicks();
}

REGISTER_WIDGET("spacewx_alerts", "SpaceWx Alerts", false, false, {
  return std::make_unique<SpaceWeatherAlertsPanel>(0, 0, 0, 0, deps.fontMgr, deps.spaceWxAlertStore);
})
