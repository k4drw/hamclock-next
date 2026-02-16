#pragma once

#include "../core/CPUMonitor.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>

struct SDL_Renderer;

// Displays CPU temperature from thermal zone
class CPUTempPanel : public Widget {
public:
  CPUTempPanel(int x, int y, int w, int h, FontManager &fontMgr,
               std::shared_ptr<CPUMonitor> monitor, bool useMetric = true);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  std::string getName() const override { return "CPUTemp"; }
  std::vector<std::string> getActions() const override { return {}; }
  SDL_Rect getActionRect(const std::string &action) const override;

private:
  FontManager &fontMgr_;
  std::shared_ptr<CPUMonitor> monitor_;
  bool useMetric_;

  float currentTemp_ = 0.0f;
  int labelFontSize_ = 12;
  int valueFontSize_ = 18;
};
