#pragma once

#include "FontManager.h"
#include "Widget.h"

#include <string>

class DEInfo : public Widget {
public:
  DEInfo(int x, int y, int w, int h, FontManager &fontMgr,
         const std::string &callsign, const std::string &grid);

  ~DEInfo() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::string callsign_;
  std::string grid_;
  double lat_ = 0.0;
  double lon_ = 0.0;

  // Four lines: "DE:" label, callsign, local time, grid + lat/lon
  static constexpr int kNumLines = 4;
  SDL_Texture *lineTex_[kNumLines] = {};
  int lineW_[kNumLines] = {};
  int lineH_[kNumLines] = {};
  std::string lineText_[kNumLines];
  std::string lastLineText_[kNumLines];

  int lineFontSize_[kNumLines] = {11, 18, 11, 11};
  int lastLineFontSize_[kNumLines] = {};
};
