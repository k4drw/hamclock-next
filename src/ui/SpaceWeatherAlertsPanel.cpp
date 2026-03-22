#include "SpaceWeatherAlertsPanel.h"
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
    return {255, 140, 200, 255}; // pink — no direct theme semantic
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

  const auto &alerts = currentData_.alerts;
  // Each alert row: type label line + issue time + (small gap)
  int rowH = rowFontSize_ + 14;
  int maxRows = (height_ - (curY - y_) - pad) / rowH;
  maxScroll_ = std::max(0, static_cast<int>(alerts.size()) - maxRows);
  scrollOffset_ = std::min(scrollOffset_, maxScroll_);

  int startIdx = scrollOffset_;
  int endIdx =
      std::min(static_cast<int>(alerts.size()), startIdx + maxRows);

  for (int i = startIdx; i < endIdx; ++i) {
    const auto &a = alerts[i];
    SDL_Color col = alertColor(a.type);

    // Color indicator dot
    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    SDL_Rect dot = {x_ + pad, curY + 3, 6, 6};
    SDL_RenderFillRect(renderer, &dot);

    // Issue time (right-aligned, truncated to MM-DD HH:MM if possible)
    if (!a.issueTime.empty()) {
      // "YYYY-MM-DD HH:MM" → show "MM-DD HH:MM" (skip year)
      std::string t = a.issueTime.size() >= 16 ? a.issueTime.substr(5) : a.issueTime;
      cat->drawText(renderer, t, x_ + width_ - pad, curY, themes.textDim,
                    FontStyle::Tiny, false, true);
    }

    // Type label
    std::string lbl = a.typeLabel;
    // Strip leading keyword to save space if it fills the label
    for (const char *pfx :
         {"ALERT: ", "WARNING: ", "WATCH: ", "SUMMARY: "}) {
      if (lbl.find(pfx) == 0) {
        lbl = lbl.substr(strlen(pfx));
        break;
      }
    }
    if (lbl.size() > 26)
      lbl = lbl.substr(0, 25) + "~";
    cat->drawText(renderer, lbl, x_ + pad + 10, curY, col, FontStyle::Fast);
    curY += rowFontSize_ + 2;

    // Summary / secondary line (dimmer, smaller)
    if (!a.summary.empty() && a.summary != a.typeLabel) {
      std::string summ = a.summary;
      for (const char *pfx :
           {"ALERT: ", "WARNING: ", "WATCH: ", "SUMMARY: "}) {
        if (summ.find(pfx) == 0) {
          summ = summ.substr(strlen(pfx));
          break;
        }
      }
      if (summ.size() > 30)
        summ = summ.substr(0, 29) + "~";
      cat->drawText(renderer, summ, x_ + pad + 10, curY, themes.textDim,
                    FontStyle::Tiny);
    }
    curY += 12;
  }

  // Scroll indicator
  if (static_cast<int>(alerts.size()) > maxRows) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d/%d", startIdx + 1,
                  static_cast<int>(alerts.size()));
    cat->drawText(renderer, buf, x_ + width_ - pad,
                  y_ + height_ - pad - rowFontSize_, themes.textDim,
                  FontStyle::Caption, false, true);
  }
}

void SpaceWeatherAlertsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  rowFontSize_ = cat ? cat->ptSize(FontStyle::Fast) : 11;
  scrollOffset_ = 0;
}

bool SpaceWeatherAlertsPanel::onMouseWheel(int scrollY) {
  scrollOffset_ = std::clamp(scrollOffset_ - scrollY, 0, maxScroll_);
  return true;
}
