#pragma once

#include "../core/RepeaterData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class RepeaterPanel : public Widget {
public:
  RepeaterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<RepeaterStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "Repeaters"; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<RepeaterStore> store_;
  RepeaterData currentData_;
  int scrollOffset_ = 0;

  int titleFontSize_ = 12;
  int rowFontSize_ = 11;
  int subFontSize_ = 9;
};
