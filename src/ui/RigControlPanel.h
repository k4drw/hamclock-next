#pragma once

#include "../core/RigData.h"
#include "FontManager.h"
#include "Widget.h"

class RigService;

// Read-only display panel for hamlib rigctld state.
// Shows VFO-A frequency, mode, S-meter bar, and connection indicator.
class RigControlPanel : public Widget {
public:
  RigControlPanel(int x, int y, int w, int h, FontManager &fontMgr,
                  RigService *rig = nullptr);

  std::string getName() const override { return "Rig Control"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  RigService *rig_;

  // Cached state updated in update()
  RigData state_;
};
