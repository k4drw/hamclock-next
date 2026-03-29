#pragma once

#include "../core/WeatherData.h"
#include "../core/WidgetType.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

// Displays a single BME280 environmental sensor reading.
// The mode_ field determines which measurement is shown.
class ENVPanel : public Widget {
public:
  ENVPanel(int x, int y, int w, int h, FontManager &fontMgr,
           std::shared_ptr<WeatherStore> store, WidgetType mode);

  std::string getName() const override { return "Environment"; }
  const char *typeId() const override {
    switch (mode_) {
      case WidgetType::ENV_TEMP:     return "env_temp";
      case WidgetType::ENV_PRESSURE: return "env_pressure";
      case WidgetType::ENV_HUMIDITY: return "env_humidity";
      case WidgetType::ENV_DEWPOINT: return "env_dewpoint";
      default:                       return "env_temp";
    }
  }
  std::string getDisplayName() const override {
    switch (mode_) {
      case WidgetType::ENV_TEMP:     return "ENV Temp";
      case WidgetType::ENV_PRESSURE: return "ENV Pressure";
      case WidgetType::ENV_HUMIDITY: return "ENV Humidity";
      case WidgetType::ENV_DEWPOINT: return "ENV Dewpoint";
      default:                       return "ENV Temp";
    }
  }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<WeatherStore> store_;
  WidgetType mode_;

  // Derive dewpoint from temperature (C) and relative humidity (%).
  // Uses Magnus formula approximation.
  static float dewpoint(float tempC, float rh);
};
