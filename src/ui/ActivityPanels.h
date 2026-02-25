#pragma once

#include "../core/ActivityData.h"
#include "../services/ActivityProvider.h"
#include "ListPanel.h"
#include <SDL.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>

class DXPedPanel : public ListPanel {
public:
  DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
             ActivityProvider &provider,
             std::shared_ptr<ActivityDataStore> store);

  void update() override;

private:
  ActivityProvider &provider_;
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  uint32_t lastFetch_ = 0;
};

class ONTAPanel : public ListPanel {
public:
  enum class Filter { ALL, POTA, SOTA };

  ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
            ActivityProvider &provider,
            std::shared_ptr<ActivityDataStore> store);
  ~ONTAPanel() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;

  // Set initial filter from persisted config value ("all", "pota", "sota").
  void setFilter(const std::string &f);

  // Called when the user changes the filter; arg is the new string value.
  void setOnFilterChanged(std::function<void(const std::string &)> cb) {
    onFilterChanged_ = std::move(cb);
  }

  void setOnSpotActivated(std::function<void(const ONTASpot &)> cb) {
    onSpotActivated_ = std::move(cb);
  }
  void setOnSpotDeactivated(std::function<void()> cb) {
    onSpotDeactivated_ = std::move(cb);
  }

  std::string getName() const override { return "ONTAPanel"; }

  // Modal setup screen
  bool isModalActive() const override;
  void renderModal(SDL_Renderer *renderer) override;

private:
  void rebuildRows(const ActivityData &data);
  static const char *filterLabel(Filter f);
  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);

  SDL_Color getRowColor(int index,
                        const SDL_Color &defaultColor) const override;

  ActivityProvider &provider_;
  std::shared_ptr<ActivityDataStore> store_;
  std::chrono::system_clock::time_point lastUpdate_{};
  uint32_t lastFetch_ = 0;

  Filter filter_ = Filter::ALL;
  Filter pendingFilter_ = Filter::ALL;
  bool showSetup_ = false;
  SDL_Rect chipRect_ = {0, 0, 0, 0};
  SDL_Rect allBtnRect_ = {0, 0, 0, 0};
  SDL_Rect potaBtnRect_ = {0, 0, 0, 0};
  SDL_Rect sotaBtnRect_ = {0, 0, 0, 0};
  SDL_Rect doneBtnRect_ = {0, 0, 0, 0};
  
  // Footer for setup button
  SDL_Texture *footerTex_ = nullptr;
  int footerW_ = 0, footerH_ = 0;
  SDL_Rect footerRect_ = {0, 0, 0, 0};

  std::function<void(const std::string &)> onFilterChanged_;
  std::function<void(const ONTASpot &)> onSpotActivated_;
  std::function<void()> onSpotDeactivated_;
  std::vector<ONTASpot> currentSpots_;

  struct CachedOntaSpot {
    SDL_Texture *modeTex = nullptr;
    int modeW = 0, modeH = 0;
    SDL_Texture *callTex = nullptr;
    int callW = 0, callH = 0;
    SDL_Texture *refTex = nullptr;
    int refW = 0, refH = 0;
    SDL_Texture *progTex = nullptr;
    int progW = 0, progH = 0;

    std::string lastMode;
    std::string lastCall;
    std::string lastRef;
    std::string lastProg;
  };
  std::vector<CachedOntaSpot> spotCache_;
  void clearSpotCache();

  static constexpr int MAX_VISIBLE_ROWS = 12; // Adjusted for ONTA panel
};
