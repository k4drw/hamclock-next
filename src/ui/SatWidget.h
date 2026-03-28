#pragma once

#include "../core/OrbitPredictor.h"
#include "../core/SatelliteManager.h"
#include "FontManager.h"
#include "SatPanel.h"
#include "SatelliteSetup.h"
#include "TextureManager.h"
#include "Widget.h"

#include <SDL.h>
#include <functional>
#include <string>
#include <vector>

// Standalone satellite tracker widget.
// Shows satellite name, AOS/LOS, Az/El, range, TLE age, and polar plot.
// Satellite selection is via a tap-to-open list menu (no DX sub-view).
// Extracted from DXSatPane's SAT mode; DX_INFO is unchanged.
class SatWidget : public Widget {
public:
  SatWidget(int x, int y, int w, int h, FontManager &fontMgr,
            TextureManager &texMgr, SatelliteManager &satMgr);
  ~SatWidget() override;

  void setObserver(double latDeg, double lonDeg);
  OrbitPredictor *activePredictor();
  const std::string &selectedSatName() const { return selectedSatName_; }

  // Restore saved satellite. Call after SatelliteManager has data.
  void restoreState(const std::string &satName);

  using SatChangedCb = std::function<void(const std::string &satName)>;
  void setOnSatChanged(SatChangedCb cb) { onSatChanged_ = std::move(cb); }

  void setMapTrackVisible(bool v) { mapTrackVisible_ = v; }
  using MapTrackToggleCb = std::function<void(bool)>;
  void setOnMapTrackToggle(MapTrackToggleCb cb) {
    onMapTrackToggle_ = std::move(cb);
  }

  bool isModalActive() const override {
    return menuOpen_ || satelliteSetup_.isActive();
  }
  bool isConfiguring() const override { return isModalActive(); }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void renderModal(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool onMouseWheel(int scrollY) override;

  void setTheme(const std::string &theme) override {
    Widget::setTheme(theme);
    satPanel_.setTheme(theme);
    satelliteSetup_.setTheme(theme);
  }
  void setMetric(bool metric) override {
    Widget::setMetric(metric);
    satPanel_.setMetric(metric);
    satelliteSetup_.setMetric(metric);
  }

  std::string getName() const override { return "SatWidget"; }
  std::string getDisplayName() const override { return "Satellite"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;

private:
  static constexpr int kActionAddSat = -1;
  static constexpr int kActionNone   = -2;

  struct MenuItem {
    std::string label;
    int action;   // kAction* or index into satSnapshot_
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
  int menuPad() const;
  int menuItemHeight() const;
  void notifySatChanged();

  FontManager     &fontMgr_;
  TextureManager  &texMgr_;
  SatelliteManager &satMgr_;

  SatPanel satPanel_;
  HamClock::SatelliteSetup satelliteSetup_;
  OrbitPredictor predictor_;

  bool menuOpen_ = false;
  int  scrollOffset_ = 0;
  int  menuFontSize_ = 14;
  std::string selectedSatName_;
  std::string pendingSatRestore_;

  std::vector<MenuItem>    menuItems_;
  std::vector<SatelliteTLE> satSnapshot_;

  SDL_Rect trackButtonRect_ = {0, 0, 0, 0};
  SDL_Rect mapTrackBtnRect_ = {0, 0, 0, 0};
  bool mapTrackVisible_ = true;

  SatChangedCb onSatChanged_;
  MapTrackToggleCb onMapTrackToggle_;
};
