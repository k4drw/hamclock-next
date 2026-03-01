#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "../core/MeteorData.h"
#include <mutex>

namespace HamClock {

class MeteorPanel : public Widget {
public:
  MeteorPanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr);

  void updateData(const MeteorData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  MeteorData data_;
  std::mutex mutex_;

  struct {
    bool visible = false;
    std::string text;
    int x, y;
  } tooltip_;

  void drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const float* values);
};

} // namespace HamClock
