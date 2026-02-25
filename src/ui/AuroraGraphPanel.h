#pragma once

#include "../core/AuroraHistoryStore.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"

#include <memory>

struct SDL_Renderer;

class AuroraGraphPanel : public Widget {
public:
  AuroraGraphPanel(int x, int y, int w, int h, FontManager &fontMgr,
                   TextureManager &texMgr,
                   std::shared_ptr<AuroraHistoryStore> store);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

private:
  void renderTooltip(SDL_Renderer *renderer);
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<AuroraHistoryStore> store_;
};
