#pragma once

#include "../core/BrightnessManager.h"
#include "../core/ConfigManager.h"
#include "FontCatalog.h"
#include "FontManager.h"
#include "TextInput.h"
#include "ThemeCustomizer.h"
#include "Widget.h"

#include <SDL.h>
#include <memory>
#include <string>
#include <vector>

class BrightnessManager;
class DisplayPower;

class SetupScreen : public Widget {
public:
  SetupScreen(int x, int y, int w, int h, FontManager &fontMgr,
              BrightnessManager &brightnessMgr,
              std::shared_ptr<DisplayPower> displayPower);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  void onMouseMove(int mx, int my) override;
  bool onMouseWheel(int scrollY) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool isModalActive() const override;
  std::string getName() const override { return "SetupScreen"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  void setConfig(const AppConfig &cfg);
  bool isComplete() const { return complete_; }
  bool wasCancelled() const { return cancelled_; }
  AppConfig getConfig(const AppConfig& base = AppConfig{}) const;

  enum class Tab {
    Identity,
    Spotting,
    Appearance,
    Rig,
    Services,
    Network,
    Widgets,
    Watchlist,
    Update
  };
  void setStartTab(Tab tab) { activeTab_ = tab; }

private:
  void recalcLayout();
  void autoPopulateLatLon();
  TextInput *getActiveInput();
  void renderTabIdentity(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                         int fieldH, int fieldX, int textPad);
  void renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                          int fieldH, int fieldX, int textPad);
  void renderTabAppearance(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                           int fieldH, int fieldX, int textPad);
  void renderTabWidgets(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                        int fieldH, int fieldX, int textPad);
  void renderTabServices(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                         int fieldH, int fieldX, int textPad);
  void renderTabRig(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                    int fieldH, int fieldX, int textPad);
  void renderTabNetwork(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                        int fieldH, int fieldX, int textPad);
  void renderTabUpdate(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                       int fieldH, int fieldX, int textPad);
  void renderTabWatchlist(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                          int fieldH, int fieldX, int textPad);

  FontManager &fontMgr_;
  BrightnessManager &brightnessMgr_;
  std::shared_ptr<DisplayPower> displayPower_;

  // Appearance tab absorbs the old Display tab (brightness/schedule now live
  // there)
  Tab activeTab_ = Tab::Identity;
  bool gpsEnabled_ = false;
  bool audioMuted_ = false;
  TextInput callsignInput_;
  TextInput gridInput_;
  TextInput latInput_;
  TextInput lonInput_;
  std::string frnText_;
  TextInput clusterHostInput_;
  TextInput clusterPortInput_;
  TextInput clusterLoginInput_;
  bool clusterEnabled_ = true;
  bool clusterWSJTX_ = false;
  TextInput wsjtxPortInput_;
  SDL_Rect wsjtxPortRect_ = {0, 0, 0, 0};
  SDL_Rect clusterHostRect_ = {0, 0, 0, 0};
  SDL_Rect clusterPortRect_ = {0, 0, 0, 0};
  SDL_Rect clusterLoginRect_ = {0, 0, 0, 0};
  SDL_Rect callsignRect_ = {0, 0, 0, 0};
  SDL_Rect gridRect_ = {0, 0, 0, 0};
  SDL_Rect latRect_ = {0, 0, 0, 0};
  SDL_Rect lonRect_ = {0, 0, 0, 0};
  bool rbnEnabled_ = false;
  bool pskOfDe_ = true;
  bool pskUseCall_ = true;
  int pskMaxAge_ = 30;
  int rotationInterval_ = 30;
  SDL_Color callsignColor_ = {255, 165, 0, 255};
  std::string panelMode_ = "dx";
  std::string selectedSatellite_;
  bool mapNightLights_ = true;
  std::string mapStyle_ = "nasa";
  std::string projection_ = "equirectangular";
  bool useMetric_ = true;
  bool rssEnabled_ = true;
  bool showBorders_ = false;
  std::string displayPowerMethod_ = "auto";
  WeatherOverlayType weatherOverlay_ = WeatherOverlayType::None;

  // Services & Rig
  TextInput qrzUsernameInput_;
  TextInput qrzPasswordInput_;
  TextInput repeaterBookInput_;
  TextInput winlinkInput_;
  std::string countdownLabel_;
  std::string countdownTime_; // YYYY-MM-DD HH:MM
  TextInput dimTimeInput_;
  TextInput brightTimeInput_;

  SDL_Rect qrzUsernameRect_ = {0, 0, 0, 0};
  SDL_Rect qrzPasswordRect_ = {0, 0, 0, 0};
  SDL_Rect repeaterBookRect_ = {0, 0, 0, 0};
  SDL_Rect winlinkRect_ = {0, 0, 0, 0};
  SDL_Rect brightTimeRect_ = {0, 0, 0, 0};
  SDL_Rect modalRect_ = {0, 0, 0, 0};

  TextInput rigHostInput_;
  TextInput rigPortInput_;
  bool rigAutoTune_ = true;

  TextInput rotatorHostInput_;
  TextInput rotatorPortInput_;
  bool rotatorAutoTrack_ = false;
  bool rotatorUpover_ = false;

  std::vector<WidgetType> paneRotations_[6];
  bool syncRotation_ = false;
  SDL_Rect syncRotationRect_ = {0, 0, 0, 0};

  // Contest Mode state
  bool contestModeActive_ = false;
  std::vector<WidgetType> contestSavedRotations_[6];
  SDL_Rect contestModeBtn_ = {0, 0, 0, 0};

  // Watchlist tab
  std::vector<std::string> watchlistEntries_;
  std::string ontaFilter_;
  SDL_Rect ontaFilterRects_[3] = {};  // 0=ALL, 1=POTA, 2=SOTA
  TextInput watchlistInputField_;
  SDL_Rect watchlistInputRect_ = {0, 0, 0, 0};
  SDL_Rect watchlistAddRect_ = {0, 0, 0, 0};
  std::vector<SDL_Rect> watchlistDeleteRects_;
  SDL_Rect watchlistScrollUpRect_ = {0, 0, 0, 0};
  SDL_Rect watchlistScrollDownRect_ = {0, 0, 0, 0};
  int watchlistScrollOffset_ = 0;
  int widgetListScrollOffset_ = 0;
  int widgetListStartY_ = 0;
  int widgetListEndY_ = 0;
  int widgetListMaxScroll_ = 0;
  SDL_Rect sidePanelModeRects_[4] = {};
  int activePane_ = 0;
  int activeField_ = 0;
  bool complete_ = false;
  bool cancelled_ = false;
  bool latLonManual_ = false;
  double gridLat_ = 0.0;
  double gridLon_ = 0.0;
  bool gridValid_ = false;
  bool mismatchWarning_ = false;
  SDL_Rect toggleRect_ = {0, 0, 0, 0};
  SDL_Rect clusterToggleRect_ = {0, 0, 0, 0};
  SDL_Rect rbnToggleRect_ = {0, 0, 0, 0};
  SDL_Rect gpsToggleRect_ = {0, 0, 0, 0};
  SDL_Rect audioMuteToggleRect_ = {0, 0, 0, 0};
  SDL_Rect themeRect_ = {0, 0, 0, 0};
  SDL_Rect nightLightsRect_ = {0, 0, 0, 0};
  SDL_Rect metricToggleRect_ = {0, 0, 0, 0};
  SDL_Rect rssToggleRect_ = {0, 0, 0, 0};
  SDL_Rect bordersToggleRect_ = {0, 0, 0, 0};
  SDL_Rect powerMethodRect_ = {0, 0, 0, 0};
  SDL_Rect weatherOverlayRect_ = {0, 0, 0, 0};  SDL_Rect mapStyleRect_ = {0, 0, 0, 0};
  SDL_Rect projectionRect_ = {0, 0, 0, 0};
  SDL_Rect rotationToggleRect_ = {0, 0, 0, 0};
  SDL_Rect paneDiagramRects_[4] = {};
  SDL_Rect okBtnRect_ = {0, 0, 0, 0};
  SDL_Rect cancelBtnRect_ = {0, 0, 0, 0};
  SDL_Rect brightnessSliderRect_ = {0, 0, 0, 0};
  SDL_Rect scheduleToggleRect_ = {0, 0, 0, 0};
  SDL_Rect dimTimeRect_ = {0, 0, 0, 0};
  SDL_Rect customizeBtnRect_ = {0, 0, 0, 0};

  std::unique_ptr<HamClock::ThemeCustomizer> themeCustomizer_;

  struct WidgetClickRect {
    WidgetType type;
    SDL_Rect rect;
  };
  std::vector<WidgetClickRect> widgetRects_;

  // Network / Hub tab
  HubMode hubMode_ = HubMode::Off;
  TextInput hubIpInput_;
  TextInput hubPortInput_;
  SDL_Rect hubModeRect_ = {0, 0, 0, 0};
  SDL_Rect hubIpRect_ = {0, 0, 0, 0};
  SDL_Rect hubPortRect_ = {0, 0, 0, 0};
  SDL_Rect rigHostRect_ = {0, 0, 0, 0};
  SDL_Rect rigPortRect_ = {0, 0, 0, 0};
  SDL_Rect rotatorHostRect_ = {0, 0, 0, 0};
  SDL_Rect rotatorPortRect_ = {0, 0, 0, 0};
  SDL_Rect rotatorAutoTrackRect_ = {0, 0, 0, 0};
  SDL_Rect rotatorUpoverRect_ = {0, 0, 0, 0};
  std::map<std::string, SDL_Color> colorOverrides_;

  // Font selection (Appearance tab, native only)
  std::string fontPath_;
#ifndef __EMSCRIPTEN__
  std::vector<std::pair<std::string, std::string>> systemFonts_; // {name, path}
  static std::vector<std::pair<std::string, std::string>> enumerateSystemFonts();
  void renderFontModal(SDL_Renderer *renderer);
#endif
  int fontListSelected_ = 0;
  SDL_Rect fontListRect_ = {};         // "Change..." button rect
#ifndef __EMSCRIPTEN__
  bool fontModalOpen_ = false;
  std::string fontModalFilter_;
  int fontModalScroll_ = 0;
  int fontModalSelected_ = 0;
  std::vector<int> fontModalFiltered_;
  SDL_Rect fontModalRect_ = {};
  SDL_Rect fontModalFilterRect_ = {};
  SDL_Rect fontModalListRect_ = {};
  SDL_Rect fontModalOkRect_ = {};
  SDL_Rect fontModalCancelRect_ = {};
#endif

  // Track dimensions to detect size changes
  int lastRenderWidth_ = 0;
  int lastRenderHeight_ = 0;
};
