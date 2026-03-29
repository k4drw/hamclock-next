#pragma once

#include "../core/WeatherData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class WeatherPanel : public Widget {
public:
  WeatherPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<WeatherStore> store, const std::string &title);

  std::string getName() const override { return "Weather"; }
  const char *typeId() const override {
    return (title_ == "DE Weather") ? "de_weather" : "dx_weather";
  }
  std::string getDisplayName() const override {
    return (title_ == "DE Weather") ? "DE Weather" : "DX Weather";
  }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<WeatherStore> store_;
  std::string title_;
  WeatherData currentData_;
  bool dataValid_ = false;
};
