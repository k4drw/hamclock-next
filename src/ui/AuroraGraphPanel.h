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
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;
  std::string getName() const override { return "Aurora Graph"; }
  const char *typeId() const override { return "aurora_graph"; }
  std::string getDisplayName() const override { return "Aurora Graph"; }

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<AuroraHistoryStore> store_;
};
