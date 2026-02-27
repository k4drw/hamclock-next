#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "../core/IonosondeData.h"
#include <mutex>

namespace HamClock {

class IonosondePanel : public Widget {
public:
  IonosondePanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr);

  void updateData(const IonosondeData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  IonosondeData data_;
  std::mutex mutex_;

  struct {
    bool visible = false;
    std::string text;
    int x, y;
  } tooltip_;

  void drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const std::vector<IonosondeStation>& stations);
  void renderTooltip(SDL_Renderer *renderer);
};

} // namespace HamClock