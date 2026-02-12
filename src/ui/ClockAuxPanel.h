#pragma once

#include "FontManager.h"
#include "Widget.h"

class ClockAuxPanel : public Widget {
public:
  ClockAuxPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  int labelFontSize_ = 12;
  int timeFontSize_ = 18;
  int infoFontSize_ = 12;
};
