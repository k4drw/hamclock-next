#pragma once

#include "../core/SolarData.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>

class SolarPanel : public Widget {
public:
  SolarPanel(int x, int y, int w, int h, FontManager &fontMgr,
             std::shared_ptr<SolarDataStore> store)
      : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

  ~SolarPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  void destroyCache() {
    if (cached_) {
      SDL_DestroyTexture(cached_);
      cached_ = nullptr;
    }
  }

  FontManager &fontMgr_;
  std::shared_ptr<SolarDataStore> store_;
  SDL_Texture *cached_ = nullptr;
  int texW_ = 0;
  int texH_ = 0;
  int fontSize_ = 24;
  std::string lastText_;
  std::string currentText_;
  int lastFontSize_ = 0;
  int valueFontSize_ = 24;
  int labelFontSize_ = 12;
};
