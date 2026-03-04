#pragma once

#include "../core/GreylineDXData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

class GreylineDXPanel : public Widget {
public:
  GreylineDXPanel(int x, int y, int w, int h, FontManager &fontMgr,
                  std::shared_ptr<GreylineDXStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  std::string getName() const override { return "GreylineDX"; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<GreylineDXStore> store_;
  GreylineDXData currentData_;
  std::string theme_ = "default";
};
