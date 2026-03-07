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

  std::string getName() const override { return "Ionosonde"; }
  void updateData(const IonosondeData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  IonosondeData data_;
  std::mutex mutex_;

  void drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const std::vector<IonosondeStation>& stations);
};

} // namespace HamClock