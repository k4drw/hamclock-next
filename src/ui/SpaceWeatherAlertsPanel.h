#pragma once

#include "../core/SpaceWeatherAlertData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class SpaceWeatherAlertsPanel : public Widget {
public:
  SpaceWeatherAlertsPanel(int x, int y, int w, int h, FontManager &fontMgr,
                          std::shared_ptr<SpaceWeatherAlertStore> store);

  std::string getName() const override { return "SpaceWxAlerts"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

private:
  SDL_Color alertColor(SpaceWxAlertType type) const;

  FontManager &fontMgr_;
  std::shared_ptr<SpaceWeatherAlertStore> store_;
  SpaceWxAlertData currentData_;
  int scrollOffset_ = 0;
  int maxScroll_ = 0;
  int rowFontSize_ = 11;
};
