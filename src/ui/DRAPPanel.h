#pragma once

#include "../services/DRAPProvider.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

#include <string>

struct SDL_Renderer;

class DRAPPanel : public Widget {
public:
  DRAPPanel(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, DRAPProvider &provider);

  std::string getName() const override { return "DRAP"; }
  const char *typeId() const override { return "drap"; }
  std::string getDisplayName() const override { return "DRAP"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  DRAPProvider &provider_;

  bool dataReady_ = false;
  uint32_t lastFetch_ = 0;
  std::string currentValue_;
};
