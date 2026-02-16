#pragma once

#include "../core/BrightnessManager.h"
#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
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
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  std::string getName() const override { return "SetupScreen"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;
  void setConfig(const AppConfig &cfg);
  bool isComplete() const { return complete_; }
  bool wasCancelled() const { return cancelled_; }
  AppConfig getConfig() const;

private:
  void recalcLayout();
  void autoPopulateLatLon();
  std::string *getActiveFieldText();
  int calculateCursorPosFromClick(int clickX, int fieldStartX,
                                  const std::string &text, int fontSize);
  void renderTabIdentity(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                         int fieldH, int fieldX, int textPad);
  void renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                          int fieldH, int fieldX, int textPad);
  void renderTabAppearance(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                           int fieldH, int fieldX, int textPad);
  void renderTabWidgets(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                        int fieldH, int fieldX, int textPad);
  void renderTabDisplay(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                        int fieldH, int fieldX, int textPad);
  void renderTabServices(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                         int fieldH, int fieldX, int textPad);
  void renderTabRig(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                    int fieldH, int fieldX, int textPad);

  FontManager &fontMgr_;
  BrightnessManager &brightnessMgr_;

  enum class Tab {
    Identity,
    Spotting,
    Appearance,
    Display,
    Rig,
    Services,
    Widgets
  };
  Tab activeTab_ = Tab::Identity;
  std::string callsignText_;
  std::string gridText_;
  std::string latText_;
  std::string lonText_;
  std::string clusterHost_;
  std::string clusterPort_;
  std::string clusterLogin_;
  bool clusterEnabled_ = true;
  bool clusterWSJTX_ = false;
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

  // Services & Rig
  std::string qrzUsername_;
  std::string qrzPassword_;
  std::string countdownLabel_;
  std::string countdownTime_; // YYYY-MM-DD HH:MM
  std::string dimTime_;
  std::string brightTime_;
  std::string rigHost_;
  std::string rigPort_;
  bool rigAutoTune_ = true;

  std::vector<WidgetType> paneRotations_[4];
  int activePane_ = 0;
  int activeField_ = 0;
  int cursorPos_ = 0;
  bool complete_ = false;
  bool cancelled_ = false;
  bool latLonManual_ = false;
  double gridLat_ = 0.0;
  double gridLon_ = 0.0;
  bool gridValid_ = false;
  bool mismatchWarning_ = false;
  int titleSize_ = 32;
  int labelSize_ = 18;
  int fieldSize_ = 24;
  int hintSize_ = 14;
  SDL_Rect toggleRect_ = {0, 0, 0, 0};
  SDL_Rect clusterToggleRect_ = {0, 0, 0, 0};
  SDL_Rect themeRect_ = {0, 0, 0, 0};
  SDL_Rect nightLightsRect_ = {0, 0, 0, 0};
  SDL_Rect metricToggleRect_ = {0, 0, 0, 0};
  SDL_Rect okBtnRect_ = {0, 0, 0, 0};
  SDL_Rect cancelBtnRect_ = {0, 0, 0, 0};
  SDL_Rect brightnessSliderRect_ = {0, 0, 0, 0};
  SDL_Rect scheduleToggleRect_ = {0, 0, 0, 0};

  struct WidgetClickRect {
    WidgetType type;
    SDL_Rect rect;
  };
  std::vector<WidgetClickRect> widgetRects_;

  // Track dimensions to detect size changes
  int lastRenderWidth_ = 0;
  int lastRenderHeight_ = 0;
};
