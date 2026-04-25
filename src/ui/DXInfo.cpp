#include "DXInfo.h"
#include "../core/Astronomy.h"
#include "../core/ConfigManager.h"
#include "../core/Constants.h"
#include "../core/CountryGrid.h"
#include "../core/GreylineCalculator.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "../services/CallbookProvider.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>

DXInfo::DXInfo(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<HamClockState> state,
               std::shared_ptr<WeatherStore> weatherStore,
               std::shared_ptr<DXClusterDataStore> dxcStore,
               CallbookProvider *callbookProvider,
               std::shared_ptr<class CallbookStore> callbookStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), callbookProvider_(callbookProvider),
      state_(std::move(state)), weatherStore_(std::move(weatherStore)),
      dxcStore_(std::move(dxcStore)), callbookStore_(std::move(callbookStore)),
      greylineModal_(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT,
                     fontMgr), manualDXInput_("") {}

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

  if (dxcStore_) {
    auto data = dxcStore_->snapshot();
    if (data->watchedSpotted) {
      auto elapsed = std::chrono::steady_clock::now() - data->watchedSpottedAt;
      watchedSpotActive_ = (elapsed < std::chrono::seconds(30));
    } else {
      watchedSpotActive_ = false;
    }
  }

  // Check for callbook data (from manual DX entry lookup)
  if (!state_->dxCallsign.empty() && callbookStore_ && state_->dxLocation.lat == 0.0) {
    auto cbData = callbookStore_->get();
    if (cbData.valid && cbData.callsign == state_->dxCallsign &&
        (cbData.lat != 0.0 || cbData.lon != 0.0)) {
      state_->dxLocation = {cbData.lat, cbData.lon};
      state_->dxGrid = cbData.grid;
    }
  }

  lineText_[0] = "DX:";

  if (!state_->dxActive) {
    lineText_[1] = "Click map or";
    lineText_[2] = "cluster spot";
    for (int i = 3; i < kNumLines; ++i)
      lineText_[i].clear();
    return;
  }

  // DX local time (line 9) — shown once async ZoneDetect lookup completes
  if (state_->dxTzValid) {
    auto now2 = std::chrono::system_clock::now();
    std::time_t t2 = std::chrono::system_clock::to_time_t(now2);
    std::time_t dxT = t2 + static_cast<std::time_t>(state_->dxTzOffset * 3600LL);
    struct tm dxTm{};
    Astronomy::portable_gmtime(&dxT, &dxTm);
    char tbuf[32];
    std::snprintf(tbuf, sizeof(tbuf), "%02d:%02d UTC%+d",
                  dxTm.tm_hour, dxTm.tm_min, state_->dxTzOffset);
    lineText_[9] = tbuf;
  } else {
    lineText_[9].clear();
  }

  if (!state_->dxCallsign.empty()) {
    lineText_[1] = state_->dxCallsign;
    if (state_->dxStartTime != std::chrono::system_clock::time_point{}) {
      auto startT = std::chrono::system_clock::to_time_t(state_->dxStartTime);
      auto endT = std::chrono::system_clock::to_time_t(state_->dxEndTime);
      struct tm tmStart, tmEnd;
      Astronomy::portable_gmtime(&startT, &tmStart);
      Astronomy::portable_gmtime(&endT, &tmEnd);
      char sBuf[16], eBuf[16], dBuf[64];
      std::strftime(sBuf, sizeof(sBuf), "%b %d", &tmStart);
      std::strftime(eBuf, sizeof(eBuf), "%b %d", &tmEnd);
      std::snprintf(dBuf, sizeof(dBuf), "%s: %s-%s", state_->dxGrid.c_str(),
                    sBuf, eBuf);
      lineText_[6] = dBuf;
    } else {
      lineText_[6] = state_->dxGrid;
    }
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
      {180, 130, 255, 255}, // DX local time — light purple
  };

  // Pane Title Label
  fontMgr_.catalog()->drawText(renderer, "DX", x_ + 10, y_ + 5, themes.accent,
                               FontStyle::MicroBold);

  // Greyline Sync Button
  int btnW = 35;
  int btnH = 16;
  if (state_->dxActive) {
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

  // Hint text when modal not active
  if (!manualDXModalActive_) {
    fontMgr_.catalog()->drawText(renderer, "(Click to enter DX manually)",
                                 x_ + pad, y_ + height_ - 30, themes.textDim,
                                 FontStyle::Micro);
  }

  // Highlight border if watched spot is active
  if (watchedSpotActive_) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, themes.warning.r, themes.warning.g,
                           themes.warning.b, 255);
    SDL_Rect rect = {x_, y_, width_, height_};
    SDL_RenderDrawRect(renderer, &rect);
    SDL_RenderDrawRect(renderer, &rect); // Double-wide border
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

  if (manualDXModalActive_) {
    // Close modal on click
    manualDXModalActive_ = false;
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

  // Click on widget to open manual entry modal
  if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
    manualDXModalActive_ = true;
    manualDXInput_.clear();
    return true;
  }

  return false;
}

bool DXInfo::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (greylineModal_.isActive()) {
    return greylineModal_.onKeyDown(key, mod);
  }

  if (manualDXModalActive_) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      // Submit callsign
      std::string val = manualDXInput_;
      std::transform(val.begin(), val.end(), val.begin(), ::toupper);
      if (!val.empty()) {
        // Set as DX target
        state_->dxCallsign = val;
        state_->dxActive = true;
        state_->dxLocation = {0.0, 0.0};
        state_->dxGrid = "";

        // Trigger callbook lookup for accurate grid
        if (callbookProvider_) {
          callbookProvider_->lookup(val);
        }

        // Track for spot matching - will update location when spot arrives
        if (dxcStore_) {
          dxcStore_->setWatchedCall(val);
        }
      }
      manualDXModalActive_ = false;
      manualDXInput_.clear();
      return true;
    }
    if (key == SDLK_ESCAPE) {
      manualDXModalActive_ = false;
      manualDXInput_.clear();
      return true;
    }
    if (key == SDLK_BACKSPACE) {
      if (!manualDXInput_.empty()) {
        manualDXInput_.pop_back();
      }
      return true;
    }
    return true; // Consume all keys when modal active
  }
  return false;
}

bool DXInfo::onTextInput(const char *text) {
  if (manualDXModalActive_) {
    // Add character to input (uppercase), limit to reasonable length
    if (manualDXInput_.length() < 20) {
      std::string upper(text);
      std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
      manualDXInput_ += upper;
    }
    return true;
  }
  return false;
}

void DXInfo::renderModal(SDL_Renderer *renderer) {
  if (greylineModal_.isActive()) {
    greylineModal_.render(renderer);
  }
  renderManualDXModal(renderer);
}

void DXInfo::renderManualDXModal(SDL_Renderer *renderer) {
  if (!manualDXModalActive_) return;

  ThemeColors themes = getThemeColors(theme_);
  int modalW = 300;
  int modalH = 100;
  int modalX = (HamClock::LOGICAL_WIDTH - modalW) / 2;
  int modalY = (HamClock::LOGICAL_HEIGHT - modalH) / 2;

  // Modal background
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_Rect modalBg = {modalX, modalY, modalW, modalH};
  SDL_RenderFillRect(renderer, &modalBg);

  // Modal border
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                         themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &modalBg);

  // Label
  fontMgr_.catalog()->drawText(renderer, "Enter DX Callsign:", modalX + 10,
                               modalY + 10, themes.text, FontStyle::Fast);

  // Input field background
  SDL_Rect inputField = {modalX + 10, modalY + 30, modalW - 20, 24};
  SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
  SDL_RenderFillRect(renderer, &inputField);
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                         themes.accent.b, 255);
  SDL_RenderDrawRect(renderer, &inputField);

  // Input text + cursor
  fontMgr_.catalog()->drawText(renderer, (manualDXInput_ + "_").c_str(),
                               modalX + 15, modalY + 35, themes.accent,
                               FontStyle::Fast);
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
                                    deps.dxWeatherStore, deps.dxcStore,
                                    deps.callbookProvider, deps.callbookStore);
  return p;
})
