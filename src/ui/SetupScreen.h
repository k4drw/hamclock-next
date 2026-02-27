#pragma once

#include "../core/BrightnessManager.h"
#include "../core/ConfigManager.h"
#include "FontCatalog.h"
#include "FontManager.h"
#include "ThemeCustomizer.h"
#include "Widget.h"

#include <SDL.h>
#include <memory>
#include <string>
#include <vector>

class BrightnessManager;

class SetupScreen : public Widget {
public:
  SetupScreen(int x, int y, int w, int h, FontManager &fontMgr,
              BrightnessManager &brightnessMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool isModalActive() const override;
  std::string getName() const override { return "SetupScreen"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  void setConfig(const AppConfig &cfg);
  bool isComplete() const { return complete_; }
  bool wasCancelled() const { return cancelled_; }
  AppConfig getConfig() const;

  enum class Tab {
    Identity,
    Spotting,
    Appearance,
    Rig,
    Services,
    Network,
    Widgets,
    Update
  };
  void setStartTab(Tab tab) { activeTab_ = tab; }

private:
  void recalcLayout();
  void autoPopulateLatLon();
  std::string *getActiveFieldText();
  bool deleteSelection(std::string *text);
  int calculateCursorPosFromClick(int clickX, int fieldStartX,
                                  const std::string &text, FontStyle style);
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

  FontManager &fontMgr_;
  BrightnessManager &brightnessMgr_;

  // Appearance tab absorbs the old Display tab (brightness/schedule now live
  // there)
  Tab activeTab_ = Tab::Identity;
  bool gpsEnabled_ = false;
  std::string callsignText_;
  std::string gridText_;
  std::string latText_;
  std::string lonText_;
  std::string frnText_;
  std::string clusterHost_;
  std::string clusterPort_;
  std::string clusterLogin_;
  bool clusterEnabled_ = true;
  bool clusterWSJTX_ = false;
  std::string wsjtxPort_;
  SDL_Rect wsjtxPortRect_ = {0, 0, 0, 0};
  bool rbnEnabled_ = false;
  bool pskOfDe_ = true;
  bool pskUseCall_ = true;
  int pskMaxAge_ = 30;
  int rotationInterval_ = 30;
  std::string theme_ = "default";
  SDL_Color callsignColor_ = {255, 165, 0, 255};
  std::string panelMode_ = "dx";
  std::string selectedSatellite_;
  bool mapNightLights_ = true;
  bool useMetric_ = true;
  bool rssEnabled_ = true;
  WeatherOverlayType weatherOverlay_ = WeatherOverlayType::None;

  // Services & Rig
  std::string qrzUsername_;
  std::string qrzPassword_;
  std::string countdownLabel_;
  std::string countdownTime_; // YYYY-MM-DD HH:MM
  std::string dimTime_;
  std::string brightTime_;

  SDL_Rect brightTimeRect_ = {0, 0, 0, 0};
  SDL_Rect modalRect_ = {0, 0, 0, 0};

  std::string rigHost_;
  std::string rigPort_;
  bool rigAutoTune_ = true;

  std::vector<WidgetType> paneRotations_[4];
  int activePane_ = 0;
  int activeField_ = 0;
  int cursorPos_ = 0;
  int selectionAnchor_ = 0;
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
  SDL_Rect themeRect_ = {0, 0, 0, 0};
  SDL_Rect nightLightsRect_ = {0, 0, 0, 0};
  SDL_Rect metricToggleRect_ = {0, 0, 0, 0};
  SDL_Rect rssToggleRect_ = {0, 0, 0, 0};
  SDL_Rect weatherOverlayRect_ = {0, 0, 0, 0};
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
  std::string hubIp_;
  std::string hubPortStr_ = "8080";
  SDL_Rect hubModeRect_ = {0, 0, 0, 0};
  SDL_Rect hubIpRect_ = {0, 0, 0, 0};
  SDL_Rect hubPortRect_ = {0, 0, 0, 0};

  // Track dimensions to detect size changes
  int lastRenderWidth_ = 0;
  int lastRenderHeight_ = 0;
};
