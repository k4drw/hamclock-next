#pragma once

#include "../core/HistoryData.h"
#include "FontManager.h"
#include "TextureManager.h"
#include "Widget.h"
#include <ctime>
#include <memory>
#include <string>

struct SDL_Renderer;

class SolarCyclePanel : public Widget {
public:
  SolarCyclePanel(int x, int y, int w, int h, FontManager &fontMgr,
                  TextureManager &texMgr, std::shared_ptr<HistoryStore> store);

  std::string getName() const override { return "SolarCycle"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  FontManager &fontMgr_;
  TextureManager &texMgr_;
  std::shared_ptr<HistoryStore> store_;
  HistorySeries currentSSN_;

  // Solar Cycle 25 reference constants
  static constexpr std::time_t kSC25StartTs = 1575158400;   // 2019-12-01 UTC
  static constexpr std::time_t kSC25PredMaxTs = 1751328000; // 2025-07-01 UTC
};
