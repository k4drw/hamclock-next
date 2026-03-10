#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"

#include <SDL.h>
#include <string>

struct SDL_Renderer;

class ClockAuxPanel : public Widget {
public:
  ClockAuxPanel(int x, int y, int w, int h, FontManager &fontMgr,
                AppConfig &config, ConfigManager &cfgMgr);
  ~ClockAuxPanel();

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;
  bool isModalActive() const override { return menuVisible_; }
  void renderModal(SDL_Renderer *renderer) override;
  std::string getName() const override { return "Clock Aux"; }

private:
  FontManager &fontMgr_;
  AppConfig &config_;
  ConfigManager &cfgMgr_;
  int labelFontSize_ = 12;
  int hmFontSize_ = 0;
  int secFontSize_ = 0;
  int infoFontSize_ = 12;
  SDL_Texture *hmTex_ = nullptr;
  SDL_Texture *secTex_ = nullptr;
  int hmW_ = 0, hmH_ = 0;
  int secW_ = 0, secH_ = 0;
  std::string lastHM_;
  std::string lastSec_;

  struct TzPreset {
    int offset;
    const char *label;
  };
  static const TzPreset kPresets[];
  static const int kPresetCount;

  // Current applied preset index (-1 = custom)
  int tzPresetIndex_ = 0;

  int sdLineY_ = 0;
  void syncFromConfig();

  // Popup menu state
  bool menuVisible_ = false;
  bool customSelected_ = false; // Custom row is selected in the menu
  int tempPresetIdx_ = 0;       // Selected preset row while menu is open
  bool offsetActive_ = true;    // Which custom text field has focus

  TextInput customOffsetInput_;
  TextInput customLabelInput_;

  // Layout rects (computed in recalcMenuLayout)
  static constexpr int kTotalRows = 10; // kPresetCount(9) + 1 Custom
  SDL_Rect menuRect_ = {};
  SDL_Rect rowRects_[kTotalRows] = {};
  SDL_Rect customOffsetRect_ = {};
  SDL_Rect customLabelRect_ = {};
  SDL_Rect doneRect_ = {};
  SDL_Rect cancelRect_ = {};

  void recalcMenuLayout();
  void renderMenu(SDL_Renderer *renderer);
};
