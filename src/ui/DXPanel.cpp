#include "DXPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <cmath>
#include <cstdio>

DXPanel::DXPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<HamClockState> state,
                 std::shared_ptr<WeatherStore> weatherStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(std::move(state)),
      weatherStore_(std::move(weatherStore)) {}

void DXPanel::destroyCache() {
  for (int i = 0; i < kNumLines; ++i) {
    if (lineTex_[i]) {
      MemoryMonitor::getInstance().destroyTexture(lineTex_[i]);
    }
  }
}

void DXPanel::update() {
  lineText_[0] = "DX:";

  if (!state_->dxActive) {
    lineText_[1] = "Select target";
    lineText_[2] = "on map";
    lineText_[3].clear();
    lineText_[4].clear();
    lineText_[5].clear();
    lineText_[6].clear();
    lineText_[7].clear();
    return;
  }

  lineText_[1] = state_->dxGrid;

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.1f%c  %.1f%c",
                std::fabs(state_->dxLocation.lat),
                state_->dxLocation.lat >= 0 ? 'N' : 'S',
                std::fabs(state_->dxLocation.lon),
                state_->dxLocation.lon >= 0 ? 'E' : 'W');
  lineText_[2] = buf;

  double bearing =
      Astronomy::calculateBearing(state_->deLocation, state_->dxLocation);
  std::snprintf(buf, sizeof(buf), "Az: %.0f%c", bearing, '\xB0'); // degree sign
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
  lineText_[5].clear(); // Was Miles equivalent, now empty

  // Weather
  if (weatherStore_) {
    auto wd = weatherStore_->get();
    if (wd.valid) {
      char wBuf[64];
      float temp = useMetric_ ? wd.temp : (wd.temp * 1.8f + 32.0f);
      const char *tempUnit = useMetric_ ? "C" : "F";
      std::snprintf(wBuf, sizeof(wBuf), "%.0f %s  %d%%", temp, tempUnit,
                    wd.humidity);
      lineText_[6] = wBuf;

      std::snprintf(wBuf, sizeof(wBuf), "%.0f hPa", wd.pressure);
      lineText_[7] = wBuf;
    } else {
      lineText_[6].clear();
      lineText_[7].clear();
    }
  } else {
    lineText_[6].clear();
    lineText_[7].clear();
  }
}

void DXPanel::render(SDL_Renderer *renderer) {
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

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int pad = static_cast<int>(width_ * 0.06f);

  // Green theme colors
  SDL_Color colors[kNumLines] = {
      {0, 255, 128, 255},   // "DX:" green
      {0, 255, 128, 255},   // Grid green
      {180, 180, 180, 255}, // Coords gray
      {255, 255, 0, 255},   // Bearing yellow
      {0, 200, 255, 255},   // Distance cyan
      {0, 200, 255, 255},   // Miles cyan
      {0, 255, 0, 255},     // Weather 1 (Green)
      {0, 255, 0, 255},     // Weather 2 (Green)
  };

  int curY = y_ + pad;
  for (int i = 0; i < kNumLines; ++i) {
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

void DXPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  lineFontSize_[0] = cat->ptSize(FontStyle::Fast); // "DX:" label
  lineFontSize_[1] = cat->ptSize(FontStyle::Fast); // Grid
  lineFontSize_[2] = cat->ptSize(FontStyle::Fast); // Coords
  lineFontSize_[3] = cat->ptSize(FontStyle::Fast); // Bearing
  lineFontSize_[4] = cat->ptSize(FontStyle::Fast); // Distance
  lineFontSize_[5] = cat->ptSize(FontStyle::Fast); // Miles
  lineFontSize_[6] = cat->ptSize(FontStyle::Fast); // Weather 1
  lineFontSize_[7] = cat->ptSize(FontStyle::Fast); // Weather 2
  lineFontSize_[7] = cat->ptSize(FontStyle::Fast); // Weather 2
  destroyCache();
}

nlohmann::json DXPanel::getDebugData() const {
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
