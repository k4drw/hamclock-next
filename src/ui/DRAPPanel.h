#pragma once

#include "../services/DRAPProvider.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

class DRAPPanel : public Widget {
public:
  DRAPPanel(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, DRAPProvider &provider);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  DRAPProvider &provider_;

  bool imageReady_ = false;
  uint32_t lastFetch_ = 0;
};
