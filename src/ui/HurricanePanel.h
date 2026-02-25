#pragma once

#include "../core/HurricaneData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class HurricanePanel : public Widget {
public:
  HurricanePanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<HurricaneStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "Hurricane"; }

private:
  static SDL_Color categoryColor(int category);

  FontManager &fontMgr_;
  std::shared_ptr<HurricaneStore> store_;
  HurricaneData currentData_;
  int scrollOffset_ = 0;

  int titleFontSize_ = 12;
  int nameFontSize_ = 14;
  int detailFontSize_ = 10;
};
