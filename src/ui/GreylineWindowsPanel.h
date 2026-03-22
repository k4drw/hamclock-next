#pragma once

#include "../core/Astronomy.h"
#include "../core/HamClockState.h"
#include "FontManager.h"
#include "Widget.h"
#include <memory>
#include <string>

struct SDL_Renderer;

class GreylineWindowsPanel : public Widget {
public:
  GreylineWindowsPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       std::shared_ptr<HamClockState> state);

  std::string getName() const override { return "GreylineWindows"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;

  SunTimes deTimes_{};
  SunTimes dxTimes_{};
  bool hasDX_ = false;
  double currentUTC_ = 0.0;
  double bearing_ = 0.0;
  double distKm_ = 0.0;

  // Half-width of a grey-line window in UTC hours (±30 min)
  static constexpr double kWindowHalfH = 0.5;

  // Returns hours until the nearest grey-line event center from nowUTC.
  static double hoursToNextEvent(double nowUTC, const SunTimes &times);
};
