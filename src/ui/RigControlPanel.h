#pragma once

#include "../core/RigData.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>

class RigService;

// Interactive rig control panel for Hamlib rigctld.
//
// Compact mode (width <= 300): read-only freq/mode/connection display.
// Expanded mode (width > 300): full interactive controls — frequency
// stepping, mode selection, VFO A/B, RIT, split, S-meter, spectrum
// placeholder. Designed to fill the map area when maximized via PaneContainer.
class RigControlPanel : public Widget {
public:
  RigControlPanel(int x, int y, int w, int h, FontManager &fontMgr,
                  RigService *rig = nullptr);

  std::string getName() const override { return "Rig Control"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;

private:
  FontManager &fontMgr_;
  RigService *rig_;
  RigData state_;

  // --- Interactive state ---
  int stepIdx_ = 3; // Index into STEP_SIZES[]; default 1 kHz (index 3)

  // --- Button rects (zero'd when not in expanded mode) ---
  SDL_Rect btnVfoA_    = {0,0,0,0};
  SDL_Rect btnVfoB_    = {0,0,0,0};
  SDL_Rect btnSwap_    = {0,0,0,0};
  SDL_Rect btnSplit_   = {0,0,0,0};
  SDL_Rect btnModes_[8]  = {};
  SDL_Rect btnSteps_[6]  = {};
  SDL_Rect btnStepDn_  = {0,0,0,0};
  SDL_Rect btnStepUp_  = {0,0,0,0};
  SDL_Rect btnRitDn_   = {0,0,0,0};
  SDL_Rect btnRitUp_   = {0,0,0,0};

  static constexpr long long STEP_SIZES[6] = {1, 10, 100, 1000, 10000, 100000};
  static constexpr long long RIT_STEP_HZ   = 100;

  void clearButtonRects();
  void renderCompact(SDL_Renderer *renderer);
  void renderExpanded(SDL_Renderer *renderer);

  static bool hitTest(const SDL_Rect &r, int mx, int my) {
    return r.w > 0 && mx >= r.x && mx < r.x + r.w &&
           my >= r.y && my < r.y + r.h;
  }
};
