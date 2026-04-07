#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "PropColorCustomizer.h"
#include "TextInput.h"
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
  void onFontChanged() override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool onMouseWheel(int scrollY) override;
  void onMouseMove(int mx, int my) override;

private:
  FontManager &fontMgr_;
  bool visible_ = false;
  AppConfig *config_ = nullptr;
  std::function<void()> onApply_;

  // Local copy of settings (for cancel support)
  std::string projection_;
  std::string mapStyle_;
  bool showGrid_;
  bool showBeacons_;
  bool showBorders_;
  bool centerMapOnDe_;
  std::string gridType_;
  PropOverlayType propOverlay_;
  WeatherOverlayType weatherOverlay_;
  std::string propColormap_;
  std::string propBand_;
  std::string propMode_;
  int propPower_;
  float propToa_;
  int propPath_;
  TextInput propToaInput_;
  
  std::vector<PropOverlayType> propRotation_;
  bool rotatingProp_ = false;

  // Combo options
  std::vector<std::string> projOpts_ = {"Equirectangular", "Robinson",
                                        "Azimuthal", "Mercator",
                                        "Dual Azimuthal"};
  std::vector<std::string> mapOpts_ = {"NASA Blue Marble", "Topo",
                                       "Topo + Bathy"};
  std::vector<std::string> gridOpts_ = {"Off", "Lat/Lon", "Maidenhead"};
  std::vector<std::string> overlayOpts_ = {"None", "MUF-RT", "VOACAP",
                                           "Reliability", "Heatmap",
                                           "DRAP", "Aurora", "Rotating..."};
  std::vector<std::string> weatherOpts_ = {"None", "WX/Pressure",
                                           "Clouds (GFS)"};
  std::vector<std::string> bandOpts_ = {"80m", "60m", "40m", "30m", "20m",
                                        "17m", "15m", "12m", "10m", "6m"};
  std::vector<std::string> modeOpts_ = {"SSB", "CW", "FT8", "AM", "WSPR"};
  std::vector<std::string> powerOpts_ = {"1W",   "5W",   "10W",
                                         "100W", "500W", "1500W"};
  std::vector<std::string> propColorOpts_ = {"Vibrant", "Muted", "Custom"};

  // Rects for dropdown HEADERS
  SDL_Rect projRec_, styleRec_;
  SDL_Rect gridRec_, overlayRec_, weatherRec_;
  SDL_Rect propColormapRec_;
  SDL_Rect beaconsRec_, bordersRec_, centerDeCheckRect_;
  SDL_Rect bandRec_, modeRec_, powerRec_; // VOACAP row
  SDL_Rect toaRec_, toaUpRec_, toaDnRec_; // TOA spinner
  SDL_Rect spRec_, lpRec_;                // Path toggles
  SDL_Rect editPropColorsRec_;            // Button next to custom prop dropdown
  
  std::vector<SDL_Rect> propRotationCheckRects_;
  int propRotationHeaderY_ = 0;

  enum {
    COMBO_PROJ,
    COMBO_STYLE,
    COMBO_GRID,
    COMBO_OVERLAY,
    COMBO_WEATHER,
    COMBO_BAND,
    COMBO_MODE,
    COMBO_POWER,
    COMBO_PROP_COLOR
  };
  // Dropdown State
  int openCombo_ = -1;
  int listScroll_ = 0;
  const int maxVisibleItems_ = 4;

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
  SDL_Rect applyRect_, cancelRect_, centerDeRect_;
  int projHeaderY_ = 0, styleHeaderY_ = 0, gridHeaderY_ = 0, mufRtHeaderY_ = 0,
      weatherHeaderY_ = 0, propColorHeaderY_ = 0, voacapHeaderY_ = 0;

  void recalcLayout();
  void renderRadioButton(SDL_Renderer *renderer, const SDL_Rect &rect,
                         bool selected, const std::string &label,
                         const SDL_Color &textColor);

  // Auto-repeat for TOA arrows
  int repeatDir_ = 0; // -1, 0, 1
  uint32_t repeatStartMs_ = 0;
  uint32_t repeatLastMs_ = 0;

  // Custom Prop Colors sub-modal
  std::unique_ptr<HamClock::PropColorCustomizer> propCustomizer_;
};
