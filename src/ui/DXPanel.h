#pragma once

#include "../core/HamClockState.h"
#include "../core/WeatherData.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>

struct SDL_Renderer;
struct SDL_Texture;

class DXPanel : public Widget {
public:
  DXPanel(int x, int y, int w, int h, FontManager &fontMgr,
          std::shared_ptr<HamClockState> state,
          std::shared_ptr<WeatherStore> weatherStore);
  ~DXPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  nlohmann::json getDebugData() const override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<WeatherStore> weatherStore_;

  // Up to 8 lines: "DX:", grid, coords, bearing, distance, (or "Select
  // target"), +2 weather lines
  static constexpr int kNumLines = 8;
  SDL_Texture *lineTex_[kNumLines] = {};
  int lineW_[kNumLines] = {};
  int lineH_[kNumLines] = {};
  std::string lineText_[kNumLines];
  std::string lastLineText_[kNumLines];

  int lineFontSize_[kNumLines] = {};
  int lastLineFontSize_[kNumLines] = {};
};
