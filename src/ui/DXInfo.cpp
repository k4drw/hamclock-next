#include "DXInfo.h"
#include "../core/Astronomy.h"
#include "../core/ConfigManager.h"
#include "../core/Constants.h"
#include "../core/CountryGrid.h"
#include "../core/GreylineCalculator.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

DXInfo::DXInfo(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<HamClockState> state,
               std::shared_ptr<WeatherStore> weatherStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(std::move(state)),
      weatherStore_(std::move(weatherStore)),
      greylineModal_(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT,
                     fontMgr) {}

void DXInfo::destroyCache() {
  for (int i = 0; i < kNumLines; ++i) {
    if (lineTex_[i]) {
      MemoryMonitor::getInstance().destroyTexture(lineTex_[i]);
    }
  }
}

void DXInfo::update() {
  if (greylineModal_.isActive())
    return;

  lineText_[0] = "DX:";

  if (!state_->dxActive) {
    lineText_[1] = "Click map or";
    lineText_[2] = "cluster spot";
    for (int i = 3; i < kNumLines; ++i)
      lineText_[i].clear();
    return;
  }

  if (!state_->dxCallsign.empty()) {
    lineText_[1] = state_->dxCallsign;
    lineText_[6] = state_->dxGrid;
  } else {
    lineText_[1] = state_->dxGrid;
    lineText_[6].clear();
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.1f%c  %.1f%c",
                std::fabs(state_->dxLocation.lat),
                state_->dxLocation.lat >= 0 ? 'N' : 'S',
                std::fabs(state_->dxLocation.lon),
                state_->dxLocation.lon >= 0 ? 'E' : 'W');
  lineText_[2] = buf;

  double bearing =
      Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
  std::snprintf(buf, sizeof(buf), "Az: %.0f°", bearing);
  lineText_[3] = buf;

  double dist =
      Astronomy::calculateDistance(state_->deLocation, state_->dxLocation);
  if (useMetric_) {
    if (dist >= 1000.0) {
      std::snprintf(buf, sizeof(buf), "Dist: %.0f km", dist);
    } else {
      std::snprintf(buf, sizeof(buf), "Dist: %.1f km", dist);
    }
  } else {
    double miles = dist * 0.621371;
    if (miles >= 1000.0) {
      std::snprintf(buf, sizeof(buf), "Dist: %.0f mi", miles);
    } else {
      std::snprintf(buf, sizeof(buf), "Dist: %.1f mi", miles);
    }
  }
  lineText_[4] = buf;

  int row = std::clamp((int)((state_->dxLocation.lat + 90.0) * 2.0), 0, 359);
  int col = std::clamp((int)((state_->dxLocation.lon + 180.0) * 2.0), 0, 719);
  uint16_t cId = COUNTRY_GRID[row][col];
  if (cId > 0 && cId < NUM_COUNTRIES) {
    lineText_[5] = COUNTRY_NAMES[cId];
  } else {
    lineText_[5].clear();
  }

  // Weather data lines
  if (weatherStore_) {
    auto wd = weatherStore_->get();
    if (wd.valid) {
      char wBuf[64];
      float temp = useMetric_ ? wd.temp : (wd.temp * 1.8f + 32.0f);
      const char *tempUnit = useMetric_ ? "C" : "F";
      std::snprintf(wBuf, sizeof(wBuf), "%.0f %s  %d%%", temp, tempUnit,
                    wd.humidity);
      lineText_[7] = wBuf;

      std::snprintf(wBuf, sizeof(wBuf), "%.0f hPa", wd.pressure);
      lineText_[8] = wBuf;
    } else {
      lineText_[7].clear();
      lineText_[8].clear();
    }
  } else {
    lineText_[7].clear();
    lineText_[8].clear();
  }
}

void DXInfo::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Standard Chrome rendering (Border and Title)
  renderChrome(renderer);

  int pad = static_cast<int>(width_ * 0.06f);

  // DX Display Colors
  SDL_Color colors[kNumLines] = {
      {0, 255, 128, 255},   // "DX:" green
      {0, 255, 128, 255},   // Callsign/Grid green
      {180, 180, 180, 255}, // Coords gray
      {255, 255, 0, 255},   // Bearing yellow
      {0, 200, 255, 255},   // Distance cyan
      {255, 128, 0, 255},   // Country orange
      {0, 255, 128, 255},   // Grid (panel-driven) green
      {0, 255, 0, 255},     // Weather 1 (Green)
      {0, 255, 0, 255},     // Weather 2 (Green)
  };

  // Pane Title Label
  fontMgr_.catalog()->drawText(renderer, "DX", x_ + 10, y_ + 5, themes.accent,
                               FontStyle::MicroBold);

  // Greyline Sync Button
  if (state_->dxActive) {
    int btnW = 35;
    int btnH = 16;
    greylineBtnRect_ = {x_ + width_ - btnW - 5, y_ + 4, btnW, btnH};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &greylineBtnRect_);
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                           themes.accent.b, 150);
    SDL_RenderDrawRect(renderer, &greylineBtnRect_);
    fontMgr_.catalog()->drawText(
        renderer, "Grey", greylineBtnRect_.x + btnW / 2,
        greylineBtnRect_.y + btnH / 2, themes.accent, FontStyle::Micro, true,
        false, true);
  } else {
    greylineBtnRect_ = {0, 0, 0, 0};
  }

  int curY = y_ + 20 + pad / 2;
  for (int i = 0; i < kNumLines; ++i) {
    if (i == 0 && lineText_[i] == "DX:")
      continue; // Handled by title
    if (lineText_[i].empty())
      continue;

    bool needRedraw = !lineTex_[i] || (lineText_[i] != lastLineText_[i]) ||
                      (lineFontSize_[i] != lastLineFontSize_[i]);
    if (needRedraw) {
      if (lineTex_[i]) {
        MemoryMonitor::getInstance().destroyTexture(lineTex_[i]);
      }
      lineTex_[i] =
          fontMgr_.renderText(renderer, lineText_[i], colors[i],
                              lineFontSize_[i], &lineW_[i], &lineH_[i]);
      lastLineText_[i] = lineText_[i];
      lastLineFontSize_[i] = lineFontSize_[i];
    }
    if (lineTex_[i]) {
      SDL_Rect dst = {x_ + pad, curY, lineW_[i], lineH_[i]};
      SDL_RenderCopy(renderer, lineTex_[i], nullptr, &dst);
      curY += lineH_[i] + pad / 3;
    }
  }
}

void DXInfo::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  for (int i = 0; i < kNumLines; ++i) {
    lineFontSize_[i] = cat->ptSize(FontStyle::Fast);
  }
  greylineModal_.onResize(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT);
  destroyCache();
}

bool DXInfo::onMouseUp(int mx, int my, Uint16 /*mod*/, int /*clicks*/) {
  if (greylineModal_.isActive()) {
    greylineModal_.onMouseUp(mx, my, 0, 1);
    return true;
  }

  // Handle Greyline button click
  if (greylineBtnRect_.w > 0 && mx >= greylineBtnRect_.x &&
      mx < greylineBtnRect_.x + greylineBtnRect_.w && my >= greylineBtnRect_.y &&
      my < greylineBtnRect_.y + greylineBtnRect_.h) {
    auto window = HamClock::GreylineCalculator::findNextOverlap(
        state_->deLocation, state_->dxLocation,
        std::chrono::system_clock::now());
    greylineModal_.setWindow(window, state_->dxCallsign);
    return true;
  }
  return false;
}

bool DXInfo::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (greylineModal_.isActive()) {
    return greylineModal_.onKeyDown(key, mod);
  }
  return false;
}

nlohmann::json DXInfo::getDebugData() const {
  nlohmann::json json;
  json["weather"] = nullptr;
  json["has_target"] = state_->dxActive;

  if (state_->dxActive && weatherStore_) {
    WeatherData wd = weatherStore_->get();
    if (wd.valid) {
      json["weather"] = {
          {"temp", wd.temp},         {"humidity", wd.humidity},
          {"pressure", wd.pressure}, {"windSpeed", wd.windSpeed},
          {"windDeg", wd.windDeg},   {"description", wd.description},
      };
    }
  }
  return json;
}

REGISTER_WIDGET("dx_info", "DX Info", false, false, {
  auto p = std::make_unique<DXInfo>(0, 0, 0, 0, deps.fontMgr, deps.state,
                                    deps.dxWeatherStore);
  return p;
})
