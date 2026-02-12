#pragma once

#include "../core/WeatherData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

class WeatherPanel : public Widget {
public:
  WeatherPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<WeatherStore> store, const std::string &title);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<WeatherStore> store_;
  std::string title_;
  WeatherData currentData_;
  bool dataValid_ = false;

  int labelFontSize_ = 12;
  int tempFontSize_ = 24;
  int infoFontSize_ = 12;
};
