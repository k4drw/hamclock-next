#pragma once

#include "../core/LiveSpotData.h"
#include "FontManager.h"
#include "Widget.h"

#include <memory>
#include <string>

class LiveSpotPanel : public Widget {
public:
  LiveSpotPanel(int x, int y, int w, int h, FontManager &fontMgr,
                std::shared_ptr<LiveSpotDataStore> store);
  ~LiveSpotPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::shared_ptr<LiveSpotDataStore> store_;

  // Snapshot of last-rendered data (to detect changes)
  int lastCounts_[kNumBands] = {};
  bool lastSelected_[kNumBands] = {};
  bool dataValid_ = false;

  // Cached textures
  SDL_Texture *titleTex_ = nullptr;
  int titleW_ = 0, titleH_ = 0;

  SDL_Texture *subtitleTex_ = nullptr;
  int subtitleW_ = 0, subtitleH_ = 0;
  std::string lastSubtitle_;

  SDL_Texture *footerTex_ = nullptr;
  int footerW_ = 0, footerH_ = 0;

  struct BandCache {
    SDL_Texture *labelTex = nullptr;
    SDL_Texture *countTex = nullptr;
    int labelW = 0, labelH = 0;
    int countW = 0, countH = 0;
    int lastCount = -1;
  };
  BandCache bandCache_[kNumBands];

  int titleFontSize_ = 14;
  int cellFontSize_ = 10;
  int lastTitleFontSize_ = 0;
  int lastCellFontSize_ = 0;

  // Cached grid geometry from last render (used by onMouseUp)
  int gridTop_ = 0;
  int gridBottom_ = 0;
  int gridCellH_ = 0;
  int gridColW_ = 0;
  int gridPad_ = 2;
};
