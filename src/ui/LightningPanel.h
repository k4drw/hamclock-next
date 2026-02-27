#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/LightningData.h"
#include <mutex>

namespace HamClock {

class LightningPanel : public Widget {
public:
  LightningPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void updateData(const LightningData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  LightningData data_;
  std::mutex mutex_;
};

} // namespace HamClock
