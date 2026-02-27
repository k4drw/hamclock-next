#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/TropoData.h"
#include <mutex>

namespace HamClock {

class TropoPanel : public Widget {
public:
  TropoPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void updateData(const TropoData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  TropoData data_;
  std::mutex mutex_;

  SDL_Color getLevelColor(TropoLevel l);
};

} // namespace HamClock
