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

  std::string getName() const override { return "DX Cluster"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;

  void setOnGreylineSync(std::function<void()> cb) { onGreylineSync_ = std::move(cb); }

  nlohmann::json getDebugData() const override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<WeatherStore> weatherStore_;
  std::function<void()> onGreylineSync_;

  SDL_Rect greylineBtnRect_ = {0, 0, 0, 0};

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
