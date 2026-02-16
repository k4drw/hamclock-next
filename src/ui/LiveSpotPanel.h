#pragma once

#include "../core/LiveSpotData.h"
#include "../services/LiveSpotProvider.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
#include <memory>
#include <string>

class LiveSpotPanel : public Widget {
public:
  LiveSpotPanel(int x, int y, int w, int h, FontManager &fontMgr,
                LiveSpotProvider &provider,
                std::shared_ptr<LiveSpotDataStore> store, AppConfig &config,
                ConfigManager &cfgMgr);
  ~LiveSpotPanel() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;

private:
  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);
  void destroyCache();

  FontManager &fontMgr_;
  LiveSpotProvider &provider_;
  std::shared_ptr<LiveSpotDataStore> store_;
  AppConfig &config_;
  ConfigManager &cfgMgr_;

  // Snapshot of last-rendered data (to detect changes)
  int lastCounts_[kNumBands] = {};
  bool lastSelected_[kNumBands] = {};
  bool dataValid_ = false;
  uint32_t lastFetch_ = 0;

  // Setup Overlay State
  bool showSetup_ = false;
  bool pendingOfDe_ = false;
  bool pendingUseCall_ = false;
  SDL_Rect modeCheckRect_ = {};
  SDL_Rect filterCheckRect_ = {};
  SDL_Rect cancelBtnRect_ = {};
  SDL_Rect doneBtnRect_ = {};

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
  SDL_Rect footerRect_ = {};
};
