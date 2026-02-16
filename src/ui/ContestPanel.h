#pragma once

#include "../core/ContestData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class ContestPanel : public Widget {
public:
  ContestPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<ContestStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<ContestStore> store_;
  ContestData currentData_;
  bool dataValid_ = false;

  int labelFontSize_ = 12;
  int itemFontSize_ = 10;
};
