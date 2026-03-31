#pragma once

#include "../core/ForecastData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class ForecastPanel : public Widget {
public:
  ForecastPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<ForecastStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "Forecast"; }
  const char *typeId() const override { return "forecast"; }
  std::string getDisplayName() const override { return "Forecast"; }
  bool isScrollable() const override { return true; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<ForecastStore> store_;
  ForecastData currentData_;
  bool useMetricTemp_ = false; // NWS data is Fahrenheit; convert if metric
  int scrollOffset_ = 0;
  int maxScroll_ = 0;

  int titleFontSize_ = 12;
  int nameFontSize_ = 12;
  int detailFontSize_ = 10;
};
