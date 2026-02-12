#pragma once

#include "../services/AuroraProvider.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

class AuroraPanel : public Widget {
public:
  AuroraPanel(int x, int y, int w, int h, FontManager &fontMgr,
              TextureManager &texMgr, AuroraProvider &provider);

  void update() override;
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  AuroraProvider &provider_;

  bool imageReady_ = false;
  uint32_t lastFetch_ = 0;
  bool north_ = true;
};
