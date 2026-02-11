#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <string>

class SetupScreen : public Widget {
public:
  SetupScreen(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Pre-populate fields from an existing config.
  void setConfig(const AppConfig &cfg);

  // True when the user has pressed Enter on a valid config.
  bool isComplete() const { return complete_; }

  // Retrieve the completed config.
  AppConfig getConfig() const;

private:
  void recalcLayout();
  void autoPopulateLatLon();

  FontManager &fontMgr_;

  // Fields: 0=callsign, 1=grid, 2=lat, 3=lon
  static constexpr int kNumFields = 4;
  int activeField_ = 0;
  std::string callsignText_;
  std::string gridText_;
  std::string latText_;
  std::string lonText_;
  int cursorPos_ = 0;
  bool complete_ = false;

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
};
