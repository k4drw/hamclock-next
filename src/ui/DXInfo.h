#pragma once

#include "../core/DXClusterData.h"
#include "../core/HamClockState.h"
#include "../core/WeatherData.h"
#include "FontManager.h"
#include "GreylineModal.h"
#include "TextInput.h"
#include "Widget.h"

#include <chrono>

#include <SDL.h>
#include <memory>
#include <string>

class PrefixManager;
class CallbookProvider;

/**
 * DXInfo: Standard DX information widget.
 * Shows DX callsign, location, grid, bearing, distance, country, and weather.
 * Includes a button to trigger the Greyline overlap modal.
 * This replaces the legacy DXSatPane's DX mode.
 */
class DXInfo : public Widget {
public:
  DXInfo(int x, int y, int w, int h, FontManager &fontMgr,
         std::shared_ptr<HamClockState> state,
         std::shared_ptr<WeatherStore> weatherStore,
         std::shared_ptr<DXClusterDataStore> dxcStore = nullptr,
         std::shared_ptr<CallbookProvider> callbookProvider = nullptr,
         std::shared_ptr<class CallbookStore> callbookStore = nullptr);
  ~DXInfo() override { destroyCache(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool onMouseWheel(int scrollY) override { return false; }

  bool isModalActive() const override { return greylineModal_.isActive() || manualDXModalActive_; }
  void renderModal(SDL_Renderer *renderer) override;

  void renderManualDXModal(SDL_Renderer *renderer);

  void setTheme(const std::string &theme) override {
    Widget::setTheme(theme);
    greylineModal_.setTheme(theme);
  }

  void setMetric(bool metric) override {
    Widget::setMetric(metric);
    greylineModal_.setMetric(metric);
  }

  std::string getName() const override { return "DXInfo"; }
  const char *typeId() const override { return "dx_info"; }
  std::string getDisplayName() const override { return "DX Info"; }

  nlohmann::json getDebugData() const override;

private:
  void destroyCache();

  FontManager &fontMgr_;
  std::shared_ptr<CallbookProvider> callbookProvider_;
  std::shared_ptr<HamClockState> state_;
  std::shared_ptr<WeatherStore> weatherStore_;
  std::shared_ptr<DXClusterDataStore> dxcStore_;
  std::shared_ptr<class CallbookStore> callbookStore_;

  HamClock::GreylineModal greylineModal_;
  SDL_Rect greylineBtnRect_ = {0, 0, 0, 0};

  // Manual DX entry modal
  bool manualDXModalActive_ = false;
  std::string manualDXInput_;
  bool watchedSpotActive_ = false;

  // Up to 10 lines: "DX:", grid, coords, bearing, distance, country, +2 weather, local time
  static constexpr int kNumLines = 10;
  SDL_Texture *lineTex_[kNumLines] = {};
  int lineW_[kNumLines] = {};
  int lineH_[kNumLines] = {};
  std::string lineText_[kNumLines];
  std::string lastLineText_[kNumLines];

  int lineFontSize_[kNumLines] = {};
  int lastLineFontSize_[kNumLines] = {};
};
