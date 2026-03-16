#include "ENVPanel.h"
#include "FontCatalog.h"
#include <cmath>
#include <cstdio>

ENVPanel::ENVPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   std::shared_ptr<WeatherStore> store, WidgetType mode)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store), mode_(mode) {}

void ENVPanel::update() {}

float ENVPanel::dewpoint(float tempC, float rh) {
  // Magnus formula constants (valid -40..60 C)
  constexpr float a = 17.62f, b = 243.12f;
  float alpha = a * tempC / (b + tempC) + std::log(rh / 100.0f);
  return b * alpha / (a - alpha);
}

void ENVPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  auto *cat = fontMgr_.catalog();
  WeatherData wd = store_ ? store_->get() : WeatherData{};

  // Title
  const char *title = "ENV";
  SDL_Color titleColor = themes.accent;
  switch (mode_) {
  case WidgetType::ENV_TEMP:
    title = "Temperature";
    break;
  case WidgetType::ENV_PRESSURE:
    title = "Pressure";
    break;
  case WidgetType::ENV_HUMIDITY:
    title = "Humidity";
    break;
  case WidgetType::ENV_DEWPOINT:
    title = "Dewpoint";
    break;
  default:
    break;
  }

  int titleH = 22;
  cat->drawText(renderer, title, x_ + 10, y_ + 5, titleColor,
                FontStyle::MicroBold);

  int centerX = x_ + width_ / 2;
  int centerY = y_ + titleH + (height_ - titleH) / 2;

  if (!wd.valid) {
    cat->drawText(renderer, "No sensor", centerX, centerY, themes.textDim,
                  FontStyle::Fast, true);
    return;
  }

  char valueBuf[32];
  switch (mode_) {
  case WidgetType::ENV_TEMP:
    std::snprintf(valueBuf, sizeof(valueBuf), "%.1f\xc2\xb0" "C", wd.temp);
    break;
  case WidgetType::ENV_PRESSURE:
    std::snprintf(valueBuf, sizeof(valueBuf), "%.1f hPa", wd.pressure);
    break;
  case WidgetType::ENV_HUMIDITY:
    std::snprintf(valueBuf, sizeof(valueBuf), "%d%%", wd.humidity);
    break;
  case WidgetType::ENV_DEWPOINT: {
    float dp = dewpoint(wd.temp, (float)wd.humidity);
    std::snprintf(valueBuf, sizeof(valueBuf), "%.1f\xc2\xb0" "C", dp);
    break;
  }
  default:
    valueBuf[0] = '\0';
    break;
  }

  cat->drawText(renderer, valueBuf, centerX, centerY, themes.text,
                FontStyle::SmallBold, true);
}

void ENVPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}
