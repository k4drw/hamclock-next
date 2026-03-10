#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "../core/SolarStormData.h"
#include <mutex>

namespace HamClock {

class SolarStormPanel : public Widget {
public:
  SolarStormPanel(int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr);

  std::string getName() const override { return "Solar Storm"; }
  void updateData(const SolarStormData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  SolarStormData data_;
  std::mutex mutex_;

  void drawSparkline(SDL_Renderer *renderer, int x, int y, int w, int h, const float* values);
  void drawBadge(SDL_Renderer *renderer, int x, int y, const std::string& label, SolarStormScale scale);
};

} // namespace HamClock
