#pragma once

#include "../core/SolarData.h"
#include "../core/Theme.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
#include <memory>

class NOAASpaceWxPanel : public Widget {
public:
  NOAASpaceWxPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   std::shared_ptr<SolarDataStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  std::string getName() const override { return "NOAA SpaceWx"; }

private:
  static SDL_Color colorForScale(int scale, const ThemeColors &themes);

  FontManager &fontMgr_;
  std::shared_ptr<SolarDataStore> store_;

  // Cached data
  int scales_[3][4] = {};
  bool dataValid_ = false;
};
