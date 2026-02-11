#pragma once

#include "../core/HamClockState.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>

class LocalPanel : public Widget {
public:
  LocalPanel(int x, int y, int w, int h, FontManager &fontMgr,
             std::shared_ptr<HamClockState> state);
  ~LocalPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;

  // 5 lines: "DE:", local time, date, grid+coords, rise/set
  static constexpr int kNumLines = 5;
  SDL_Texture *lineTex_[kNumLines] = {};
  int lineW_[kNumLines] = {};
  int lineH_[kNumLines] = {};
  std::string lineText_[kNumLines];
  std::string lastLineText_[kNumLines];

  int lineFontSize_[kNumLines] = {};
  int lastLineFontSize_[kNumLines] = {};

  // Seconds display (superscript, drawn next to lineText_[1])
  std::string currentSec_;
  std::string lastSec_;
  SDL_Texture *secTex_ = nullptr;
  int secW_ = 0;
  int secH_ = 0;
  int secFontSize_ = 0;
  int lastSecFontSize_ = 0;
};
