#pragma once

#include "../services/AuroraProvider.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

struct SDL_Renderer;

class AuroraPanel : public Widget {
public:
  AuroraPanel(int x, int y, int w, int h, FontManager &fontMgr,
              TextureManager &texMgr, AuroraProvider &provider);

  std::string getName() const override { return "Aurora"; }
  const char *typeId() const override { return "aurora"; }
  std::string getDisplayName() const override { return "Aurora"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  AuroraProvider &provider_;

  bool imageReady_ = false;
  uint32_t lastFetch_ = 0;
  bool north_ = true;
};
