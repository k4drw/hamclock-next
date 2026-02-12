#pragma once

#include "../core/MoonData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

class MoonPanel : public Widget {
public:
  MoonPanel(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<MoonStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<MoonStore> store_;
  MoonData currentData_;
  bool dataValid_ = false;

  void drawMoon(SDL_Renderer *renderer, int cx, int cy, int r, double phase);

  int labelFontSize_ = 12;
  int valueFontSize_ = 14;
};
