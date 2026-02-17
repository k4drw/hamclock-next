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

  bool isModalActive() const override { return showSetup_; }
  void renderModal(SDL_Renderer *renderer) override { renderSetup(renderer); }

  std::string getName() const override { return "LiveSpots"; }
  std::vector<std::string> getActions() const override;
  bool performAction(const std::string &action) override;
  SDL_Rect getActionRect(const std::string &action) const override;
  nlohmann::json getDebugData() const override;

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
  LiveSpotSource activeTab_ = LiveSpotSource::PSK;

  // Pending settings for the currently active tab in the setup UI
  bool pendingOfDe_ = false;
  bool pendingUseCall_ = false;
  int pendingMaxAge_ = 30;

  // UI Rects for setup
  SDL_Rect tabRects_[3] = {}; // PSK, RBN, WSPR
  SDL_Rect modeCheckRect_ = {};
  SDL_Rect filterCheckRect_ = {};
  SDL_Rect ageIncrRect_ = {};
  SDL_Rect ageDecrRect_ = {};
  SDL_Rect cancelBtnRect_ = {};
  SDL_Rect doneBtnRect_ = {};
  SDL_Rect menuRect_ = {};

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
