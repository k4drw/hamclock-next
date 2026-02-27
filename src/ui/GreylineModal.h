#pragma once

#include "Widget.h"
#include "FontManager.h"
#include "../core/GreylineCalculator.h"
#include <SDL.h>

namespace HamClock {

class GreylineModal : public Widget {
public:
  GreylineModal(int x, int y, int w, int h, FontManager &fontMgr);

  void setWindow(const GreylineWindow &window, const std::string &dxName);
  void setActive(bool active) { active_ = active; }
  bool isActive() const { return active_; }

  void update() override {}
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

private:
  FontManager &fontMgr_;
  GreylineWindow window_;
  std::string dxName_;
  bool active_ = false;
  SDL_Rect dialogRect_;
  SDL_Rect okRect_;

  void calculateLayout();
};

} // namespace HamClock
