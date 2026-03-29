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

  std::string getName() const override { return "Meteor Scatter"; }
  const char *typeId() const override { return "meteor"; }
  std::string getDisplayName() const override { return "Meteor Scatter"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void updateData(const MeteorData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  MeteorData data_;
  std::mutex mutex_;

  void drawGraph(SDL_Renderer *renderer, int x, int y, int w, int h, const float* values);
};

} // namespace HamClock
