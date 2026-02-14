#include "LocalPanel.h"
#include "../core/Astronomy.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>

LocalPanel::LocalPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       std::shared_ptr<HamClockState> state,
                       std::shared_ptr<WeatherStore> weatherStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(std::move(state)),
      weatherStore_(std::move(weatherStore)) {}

void LocalPanel::destroyCache() {
  for (int i = 0; i < kNumLines; ++i) {
    if (lineTex_[i]) {
      SDL_DestroyTexture(lineTex_[i]);
      lineTex_[i] = nullptr;
    }
  }
  if (secTex_) {
    SDL_DestroyTexture(secTex_);
    secTex_ = nullptr;
  }
}

void LocalPanel::update() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
  Astronomy::portable_gmtime(&t, &utc);

  // Crude local time offset from longitude (truncate toward zero)
  double lon = state_->deLocation.lon;
  int utcOffset = static_cast<int>(lon / 15.0);

  int localHour = (utc.tm_hour + utcOffset + 24) % 24;
  int localMin = utc.tm_min;

  lineText_[0] = "DE:";

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%02d:%02d", localHour, localMin);
  lineText_[1] = buf;

  std::snprintf(buf, sizeof(buf), "%02d", utc.tm_sec);
  currentSec_ = buf;

  static constexpr const char *kDays[] = {"Sun", "Mon", "Tue", "Wed",
                                          "Thu", "Fri", "Sat"};
  static constexpr const char *kMonths[] = {"Jan", "Feb", "Mar", "Apr",
                                            "May", "Jun", "Jul", "Aug",
                                            "Sep", "Oct", "Nov", "Dec"};
  std::snprintf(buf, sizeof(buf), "%s, %d %s %04d", kDays[utc.tm_wday],
                utc.tm_mday, kMonths[utc.tm_mon], 1900 + utc.tm_year);
  lineText_[2] = buf;

  // Sunrise / Sunset
  int doy = utc.tm_yday + 1;
  SunTimes st = Astronomy::calculateSunTimes(state_->deLocation.lat,
                                             state_->deLocation.lon, doy);

  if (st.hasRise && st.hasSet) {
    // Convert UTC sun times to local
    double localRise = st.sunrise + utcOffset;
    double localSet = st.sunset + utcOffset;
    auto norm24 = [](double &h) {
      while (h < 0.0)
        h += 24.0;
      while (h >= 24.0)
        h -= 24.0;
    };
    norm24(localRise);
    norm24(localSet);
    int rH = static_cast<int>(localRise);
    int rM = static_cast<int>((localRise - rH) * 60);
    int sH = static_cast<int>(localSet);
    int sM = static_cast<int>((localSet - sH) * 60);
    std::snprintf(buf, sizeof(buf), "R %02d:%02d  S %02d:%02d", rH, rM, sH, sM);
  } else {
    std::snprintf(buf, sizeof(buf), "No rise/set");
  }
  lineText_[3] = buf;

  // Weather data
  if (weatherStore_) {
    WeatherData wd = weatherStore_->get();
    if (wd.valid) {
      char wBuf[64];
      float temp = useMetric_ ? wd.temp : (wd.temp * 1.8f + 32.0f);
      const char *tempUnit = useMetric_ ? "C" : "F";
      std::snprintf(wBuf, sizeof(wBuf), "%.0f %s  %d%%", temp, tempUnit,
                    wd.humidity);
      lineText_[4] = wBuf;

      std::snprintf(wBuf, sizeof(wBuf), "%.0f hPa", wd.pressure);
      lineText_[5] = wBuf;
    } else {
      lineText_[4] = "";
      lineText_[5] = "";
    }
  } else {
    lineText_[4] = "";
    lineText_[5] = "";
  }
}

void LocalPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Clip to widget bounds
  SDL_Rect clip = {x_, y_, width_, height_};
  SDL_RenderSetClipRect(renderer, &clip);

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_RenderFillRect(renderer, &clip);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &clip);

  int pad = static_cast<int>(width_ * 0.06f);

  // Orange theme colors
  SDL_Color colors[kNumLines] = {
      {0, 200, 255, 255},   // "DE:" cyan
      {255, 165, 0, 255},   // Local time orange (large)
      {255, 255, 255, 255}, // Date white
      {0, 255, 128, 255},   // Rise/set green
      {255, 165, 0, 255},   // Weather orange
      {255, 165, 0, 255},   // Weather orange
  };

  int curY = y_ + pad;
  for (int i = 0; i < kNumLines; ++i) {
    bool needRedraw = !lineTex_[i] || (lineText_[i] != lastLineText_[i]) ||
                      (lineFontSize_[i] != lastLineFontSize_[i]);
    if (needRedraw) {
      if (lineTex_[i]) {
        SDL_DestroyTexture(lineTex_[i]);
        lineTex_[i] = nullptr;
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

      // Add seconds superscript for the time line
      if (i == 1) {
        bool needSecRedraw = !secTex_ || (currentSec_ != lastSec_) ||
                             (secFontSize_ != lastSecFontSize_);
        if (needSecRedraw) {
          if (secTex_) {
            SDL_DestroyTexture(secTex_);
            secTex_ = nullptr;
          }
          SDL_Color secColor = colors[1];
          secTex_ = fontMgr_.renderText(renderer, currentSec_, secColor,
                                        secFontSize_, &secW_, &secH_);
          lastSec_ = currentSec_;
          lastSecFontSize_ = secFontSize_;
        }
        if (secTex_) {
          SDL_Rect secDst = {x_ + pad + lineW_[i] + 2, curY, secW_, secH_};
          SDL_RenderCopy(renderer, secTex_, nullptr, &secDst);
        }
      }

      curY += lineH_[i] + pad / 3;
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

void LocalPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  int fast = cat->ptSize(FontStyle::Fast);
  int fastBold = cat->ptSize(FontStyle::FastBold);
  int clockPt = std::clamp(static_cast<int>(height_ * 0.25f), 24, 60);

  lineFontSize_[0] = fast;     // "DE:" label
  lineFontSize_[1] = clockPt;  // Local time (large)
  lineFontSize_[2] = fast;     // Date
  lineFontSize_[3] = fastBold; // Rise/set
  lineFontSize_[4] = fast;     // Weather 1
  lineFontSize_[5] = fast;     // Weather 2
  secFontSize_ = fastBold;     // Seconds (superscript)
  destroyCache();
}

nlohmann::json LocalPanel::getDebugData() const {
  nlohmann::json json;
  json["weather"] = nullptr;
  if (weatherStore_) {
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
