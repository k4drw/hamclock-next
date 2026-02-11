#pragma once

#include "../core/HamClockState.h"
#include "../core/OrbitPredictor.h"
#include "../core/SatelliteManager.h"
#include "DXPanel.h"
#include "FontManager.h"
#include "SatPanel.h"
#include "Widget.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class DXSatPane : public Widget {
public:
  enum class Mode { DX, SAT };

  DXSatPane(int x, int y, int w, int h, FontManager &fontMgr,
            std::shared_ptr<HamClockState> state, SatelliteManager &satMgr);
  ~DXSatPane() override;

  // Set observer location for orbit predictions.
  void setObserver(double latDeg, double lonDeg);

  // Returns predictor when in SAT mode with a loaded satellite, nullptr
  // otherwise.
  OrbitPredictor *activePredictor();

  Mode mode() const { return mode_; }
  const std::string &selectedSatName() const { return selectedSatName_; }

  // Restore saved panel state. Call after SatelliteManager has data.
  // If satName is non-empty, attempts to load that satellite and switch to SAT
  // mode.
  void restoreState(const std::string &panelMode, const std::string &satName);

  // Called when mode or satellite selection changes.
  using ModeChangedCb =
      std::function<void(const std::string &mode, const std::string &satName)>;
  void setOnModeChanged(ModeChangedCb cb) { onModeChanged_ = std::move(cb); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onMouseWheel(int scrollY) override;

private:
  // Menu has two views: the SAT options menu and the satellite list
  enum class MenuState { Closed, SatOptions, SatList };

  static constexpr int kActionChooseSats = -1;
  static constexpr int kActionShowDX = -2;
  static constexpr int kActionNone = -3;

  struct MenuItem {
    std::string label;
    int action; // kAction* constant or index into satSnapshot_
    bool selected;
    SDL_Texture *tex = nullptr;
    int texW = 0, texH = 0;
  };

  void openMenu();
  void closeMenu();
  void populateMenu();
  void destroyMenuTextures();
  void handleMenuClick(int mx, int my);
  void executeAction(int action);
  void renderMenu(SDL_Renderer *renderer);
  void drawRadio(SDL_Renderer *renderer, int cx, int cy, int r, bool filled);
  int menuItemHeight() const;
  int menuPad() const;

  FontManager &fontMgr_;
  std::shared_ptr<HamClockState> state_;
  SatelliteManager &satMgr_;

  Mode mode_ = Mode::DX;
  MenuState menuState_ = MenuState::Closed;
  int scrollOffset_ = 0;
  std::string selectedSatName_;

  DXPanel dxPanel_;
  SatPanel satPanel_;
  OrbitPredictor predictor_;

  void notifyModeChanged();

  std::vector<MenuItem> menuItems_;
  std::vector<SatelliteTLE> satSnapshot_;
  int menuFontSize_ = 14;

  ModeChangedCb onModeChanged_;
  std::string pendingSatRestore_; // satellite name to restore when data arrives
};
