#pragma once

#include "../core/CallbookData.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class CallbookPanel : public Widget {
public:
  CallbookPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<CallbookStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<CallbookStore> store_;
  CallbookData currentData_;

  int titleSize_ = 14;
  int nameSize_ = 18;
  int infoSize_ = 12;
};
