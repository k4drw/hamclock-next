#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/LightningData.h"
#include <mutex>

namespace HamClock {

class LightningPanel : public Widget {
public:
  LightningPanel(int x, int y, int w, int h, FontManager &fontMgr);

  std::string getName() const override { return "Lightning"; }
  const char *typeId() const override { return "lightning"; }
  std::string getDisplayName() const override { return "Lightning"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void updateData(const LightningData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;

private:
  FontManager &fontMgr_;
  LightningData data_;
  std::mutex mutex_;
};

} // namespace HamClock
