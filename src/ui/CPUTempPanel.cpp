#include "CPUTempPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <SDL.h>
#include <cstdio>

CPUTempPanel::CPUTempPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<CPUMonitor> monitor, bool useMetric)
    : Widget(x, y, w, h), fontMgr_(fontMgr), monitor_(std::move(monitor)),
      useMetric_(useMetric) {}

void CPUTempPanel::update() {
  if (monitor_ && monitor_->isAvailable()) {
    currentTemp_ = useMetric_ ? monitor_->getTemperature()
                               : monitor_->getTemperatureF();
  } else {
    currentTemp_ = 0.0f;
  }
}

void CPUTempPanel::render(SDL_Renderer *renderer) {
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

  if (!monitor_ || !monitor_->isAvailable()) {
    fontMgr_.drawText(renderer, "No CPU Temp", x_ + width_ / 2,
                      y_ + height_ / 2, themes.textDim, labelFontSize_, false,
                      true);
    return;
  }

  // Label
  fontMgr_.drawText(renderer, "CPU", x_ + width_ / 2, y_ + 8, themes.accent,
                    labelFontSize_, true, true);

  // Temperature value with color coding
  char buf[32];
  const char *unit = useMetric_ ? "C" : "F";
  std::snprintf(buf, sizeof(buf), "%.1f°%s", currentTemp_, unit);

  // Color based on temperature (Celsius thresholds)
  float tempC = useMetric_ ? currentTemp_ : (currentTemp_ - 32.0f) * 5.0f / 9.0f;
  SDL_Color tempColor;
  if (tempC < 50.0f) {
    tempColor = {0, 255, 0, 255}; // Green: Cool
  } else if (tempC < 70.0f) {
    tempColor = {255, 255, 0, 255}; // Yellow: Warm
  } else if (tempC < 85.0f) {
    tempColor = {255, 165, 0, 255}; // Orange: Hot
  } else {
    tempColor = {255, 0, 0, 255}; // Red: Very hot
  }

  fontMgr_.drawText(renderer, buf, x_ + width_ / 2, y_ + height_ / 2 + 5,
                    tempColor, valueFontSize_, false, true);
}

void CPUTempPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (cat) {
    labelFontSize_ = cat->ptSize(FontStyle::Fast);
    valueFontSize_ = cat->ptSize(FontStyle::SmallBold);

    if (h < 60) {
      valueFontSize_ = cat->ptSize(FontStyle::Fast);
    }
  }
}

SDL_Rect CPUTempPanel::getActionRect(const std::string &action) const {
  (void)action; // No actions for this panel
  return {x_, y_, width_, height_};
}
