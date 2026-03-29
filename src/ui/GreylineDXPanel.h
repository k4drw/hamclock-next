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
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "Greyline DX"; }
  const char *typeId() const override { return "greyline_dx"; }
  std::string getDisplayName() const override { return "Greyline DX"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<GreylineDXStore> store_;
  GreylineDXData currentData_;
  int scrollOffset_ = 0;
  int maxScroll_ = 0;
};
