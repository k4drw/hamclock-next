#pragma once

#include "../core/WeatherData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

// Displays a single BME280 environmental sensor reading.
// The mode_ field determines which measurement is shown.
class ENVPanel : public Widget {
public:
  enum class ENVMode { Temp, Pressure, Humidity, Dewpoint };

  ENVPanel(int x, int y, int w, int h, FontManager &fontMgr,
           std::shared_ptr<WeatherStore> store, ENVMode mode);

  std::string getName() const override { return "Environment"; }
  const char *typeId() const override {
    switch (mode_) {
      case ENVMode::Temp:     return "env_temp";
      case ENVMode::Pressure: return "env_pressure";
      case ENVMode::Humidity: return "env_humidity";
      case ENVMode::Dewpoint: return "env_dewpoint";
    }
    return "env_temp";
  }
  std::string getDisplayName() const override {
    switch (mode_) {
      case ENVMode::Temp:     return "ENV Temp";
      case ENVMode::Pressure: return "ENV Pressure";
      case ENVMode::Humidity: return "ENV Humidity";
      case ENVMode::Dewpoint: return "ENV Dewpoint";
    }
    return "ENV Temp";
  }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<WeatherStore> store_;
  ENVMode mode_;

  // Derive dewpoint from temperature (C) and relative humidity (%).
  // Uses Magnus formula approximation.
  static float dewpoint(float tempC, float rh);
};
