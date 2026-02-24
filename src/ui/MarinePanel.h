#pragma once

#include "../core/MarineData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class MarinePanel : public Widget {
public:
  MarinePanel(int x, int y, int w, int h, FontManager &fontMgr,
              std::shared_ptr<MarineStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "Marine"; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<MarineStore> store_;
  MarineData currentData_;

  int titleFontSize_ = 12;
  int rowFontSize_ = 11;
  int subFontSize_ = 9;
};
