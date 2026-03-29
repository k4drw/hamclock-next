#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/TropoData.h"
#include <mutex>

namespace HamClock {

class TropoPanel : public Widget {
public:
  TropoPanel(int x, int y, int w, int h, FontManager &fontMgr);

  std::string getName() const override { return "Tropo"; }
  const char *typeId() const override { return "tropo"; }
  std::string getDisplayName() const override { return "Tropo"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void updateData(const TropoData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  TropoData data_;
  std::mutex mutex_;

  SDL_Color getLevelColor(TropoLevel l);
};

} // namespace HamClock
