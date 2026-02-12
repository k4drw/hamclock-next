#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <chrono>
#include <string>

class CountdownPanel : public Widget {
public:
  CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  std::chrono::system_clock::time_point targetTime_;
  std::string label_ = "Field Day 2026";
};
