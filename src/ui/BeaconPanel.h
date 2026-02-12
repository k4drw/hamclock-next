#pragma once

#include "../services/BeaconProvider.h"
#include "FontManager.h"
#include "Widget.h"

class BeaconPanel : public Widget {
public:
  BeaconPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  BeaconProvider provider_;

  std::vector<ActiveBeacon> active_;
  float progress_ = 0;

  int labelFontSize_ = 10;
  int callfontSize_ = 11;
};
