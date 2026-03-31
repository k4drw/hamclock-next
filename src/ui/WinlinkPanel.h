#pragma once

#include "../core/WinlinkData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class WinlinkPanel : public Widget {
public:
  WinlinkPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<WinlinkStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseWheel(int scrollY) override;

  std::string getName() const override { return "Winlink"; }
  const char *typeId() const override { return "winlink"; }
  std::string getDisplayName() const override { return "Winlink"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return true; }

private:
  FontManager &fontMgr_;
  std::shared_ptr<WinlinkStore> store_;
  WinlinkData currentData_;
  int scrollOffset_ = 0;
  int maxScroll_ = 0;

  int titleFontSize_ = 12;
  int rowFontSize_ = 11;
  int subFontSize_ = 9;
};
