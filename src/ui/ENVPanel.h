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
