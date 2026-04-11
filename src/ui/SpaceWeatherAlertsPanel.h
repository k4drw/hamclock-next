#pragma once

#include "../core/ConfigManager.h"
#include "../core/SpaceWeatherAlertData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class SpaceWeatherAlertsPanel : public Widget {
public:
  SpaceWeatherAlertsPanel(int x, int y, int w, int h, FontManager &fontMgr,
                          AppConfig &config,
                          std::shared_ptr<SpaceWeatherAlertStore> store);

  std::string getName() const override { return "SpaceWxAlerts"; }
  const char *typeId() const override { return "spacewx_alerts"; }
  std::string getDisplayName() const override { return "SpaceWx Alerts"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;
  void onMouseMove(int mx, int my) override;

private:
  SDL_Color alertColor(SpaceWxAlertType type) const;

  FontManager &fontMgr_;
  AppConfig &config_;
  std::shared_ptr<SpaceWeatherAlertStore> store_;
  SpaceWxAlertData currentData_;
  int scrollOffset_ = 0;
  int maxScroll_ = 0;
  int rowFontSize_ = 11;
};
