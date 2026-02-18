#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>

#include <functional>
#include <string>

class MapViewMenu : public Widget {
public:
  MapViewMenu(FontManager &fontMgr);

  void show(AppConfig &config, std::function<void()> onApply);
  void hide();
  bool isVisible() const { return visible_; }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onMouseWheel(int scrollY) override;

  void setTheme(const std::string &theme) override { theme_ = theme; }

private:
  FontManager &fontMgr_;
  bool visible_ = false;
  AppConfig *config_ = nullptr;
  std::function<void()> onApply_;

  // Local copy of settings (for cancel support)
  std::string projection_;
  std::string mapStyle_;
  bool showGrid_;
  std::string gridType_;
  PropOverlayType propOverlay_;
  std::string propBand_;
  std::string propMode_;
  int propPower_;

  // Combo options
  std::vector<std::string> projOpts_ = {"Equirectangular", "Robinson",
                                        "Mercator"};
  std::vector<std::string> mapOpts_ = {"NASA Blue Marble", "Topo",
                                       "Topo + Bathy"};
  std::vector<std::string> gridOpts_ = {"Off", "Lat/Lon", "Maidenhead"};
  std::vector<std::string> overlayOpts_ = {"None", "MUF", "VOACAP"};
  std::vector<std::string> bandOpts_ = {"80m", "60m", "40m", "30m", "20m",
                                        "17m", "15m", "12m", "10m", "6m"};
  std::vector<std::string> modeOpts_ = {"SSB", "CW", "FT8", "AM", "WSPR"};
  std::vector<std::string> powerOpts_ = {"1W",   "5W",   "10W",
                                         "100W", "500W", "1500W"};

  // Rects for dropdown HEADERS
  SDL_Rect projRec_, styleRec_;
  SDL_Rect gridRec_, overlayRec_;
  SDL_Rect bandRec_, modeRec_, powerRec_; // VOACAP row

  enum {
    COMBO_PROJ,
    COMBO_STYLE,
    COMBO_GRID,
    COMBO_OVERLAY,
    COMBO_BAND,
    COMBO_MODE,
    COMBO_POWER
  };
  // Dropdown State
  int openCombo_ = -1;
  int listScroll_ = 0;
  const int maxVisibleItems_ = 6;

  void drawDropdown(SDL_Renderer *renderer, const SDL_Rect &rect,
                    const std::string &currentVal, bool isOpen);
  void drawDropdownList(SDL_Renderer *renderer, const SDL_Rect &headerRect,
                        const std::vector<std::string> &opts);
  int getDropdownHeight(int numOpts);

  // Menu layout
  SDL_Rect menuRect_;
  // SDL_Rect projEquiRect_, projRobinsonRect_, projMercatorRect_; // REMOVED
  // SDL_Rect styleNasaRect_, styleTerrainRect_, styleCountriesRect_; // REMOVED
  // SDL_Rect gridOffRect_, gridLatLonRect_, gridMaidenheadRect_; // REMOVED
  // SDL_Rect propNoneRect_, propMufRect_, propVoacapRect_; // REMOVED
  // SDL_Rect pb80_, pb40_, pb20_, pb15_, pb10_; // REMOVED
  SDL_Rect applyRect_, cancelRect_;
  int projHeaderY_ = 0, styleHeaderY_ = 0, gridHeaderY_ = 0, mufRtHeaderY_ = 0;

  std::string theme_ = "default";

  void renderRadioButton(SDL_Renderer *renderer, const SDL_Rect &rect,
                         bool selected, const std::string &label,
                         const SDL_Color &textColor);
};
