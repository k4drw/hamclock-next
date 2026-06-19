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
  const char *typeId() const override { return "ionosonde"; }
  std::string getDisplayName() const override { return "Ionosonde"; }
  bool isScrollable() const override { return false; }
  bool requiresConfigKey() const override { return false; }
  void updateData(const IonosondeData &data);
  void update() override {}
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  IonosondeData data_;
  std::mutex mutex_;
};

} // namespace HamClock