#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <string>
#include <vector>

class SetupScreen : public Widget {
public:
  SetupScreen(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Semantic Debug API
  std::string getName() const override { return "SetupScreen"; }
  std::vector<std::string> getActions() const override;
  SDL_Rect getActionRect(const std::string &action) const override;

  // Pre-populate fields from an existing config.
  void setConfig(const AppConfig &cfg);

  // True when the user has pressed Enter on a valid config.
  bool isComplete() const { return complete_; }
  bool wasCancelled() const { return cancelled_; }

  // Retrieve the completed config.
  AppConfig getConfig() const;

private:
  void recalcLayout();
  void autoPopulateLatLon();
  void renderTabIdentity(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                         int fieldH, int fieldX, int textPad);
  void renderTabDXCluster(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                          int fieldH, int fieldX, int textPad);
  void renderTabAppearance(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                           int fieldH, int fieldX, int textPad);
  void renderTabWidgets(SDL_Renderer *renderer, int cx, int pad, int fieldW,
                        int fieldH, int fieldX, int textPad);

  FontManager &fontMgr_;

  // Tabs
  enum class Tab { Identity, DXCluster, Appearance, Widgets };
  Tab activeTab_ = Tab::Identity;

  // Identity Fields
  std::string callsignText_;
  std::string gridText_;
  std::string latText_;
  std::string lonText_;

  // Network Fields
  std::string clusterHost_;
  std::string clusterPort_;
  std::string clusterLogin_;
  bool clusterEnabled_ = true;
  bool clusterWSJTX_ = false;

  // Appearance
  int rotationInterval_ = 30;
  std::string theme_ = "default";
  SDL_Color callsignColor_ = {255, 165, 0, 255};
  std::string panelMode_ = "dx";
  std::string selectedSatellite_;

  // Widgets
  std::vector<WidgetType> paneRotations_[4];
  int activePane_ =
      0; // The pane (0-3) currently being edited in the Widgets tab

  int activeField_ = 0; // 0..3 for Identity, 0..2 for Network, etc.

  int cursorPos_ = 0;
  bool complete_ = false;
  bool cancelled_ = false;

  // Whether lat/lon were manually edited (suppresses auto-populate)
  bool latLonManual_ = false;

  // Grid-computed lat/lon for mismatch detection
  double gridLat_ = 0.0;
  double gridLon_ = 0.0;
  bool gridValid_ = false;

  // Warning when manual lat/lon falls outside the entered grid
  bool mismatchWarning_ = false;

  // Layout metrics (recomputed on resize)
  int titleSize_ = 32;
  int labelSize_ = 18;
  int fieldSize_ = 24;
  int hintSize_ = 14;
  SDL_Rect toggleRect_ = {0, 0, 0, 0};
  SDL_Rect clusterToggleRect_ = {0, 0, 0, 0};
  SDL_Rect themeRect_ = {0, 0, 0, 0};

  // Row of widget rects for current pane
  struct WidgetClickRect {
    WidgetType type;
    SDL_Rect rect;
  };
  std::vector<WidgetClickRect> widgetRects_;
};
