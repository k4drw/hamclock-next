#pragma once

#include "../core/DstData.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <memory>

struct SDL_Renderer;

class DstPanel : public Widget {
public:
  DstPanel(int x, int y, int w, int h, FontManager &fontMgr,
           TextureManager &texMgr, std::shared_ptr<DstStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

  std::string getName() const override { return "DstPanel"; }
  nlohmann::json getDebugData() const override;

private:
  void renderTooltip(SDL_Renderer *renderer);
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<DstStore> store_;
  DstData currentData_;
};
