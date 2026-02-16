#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <string>

class DXClusterSetup : public Widget {
public:
  DXClusterSetup(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  void setConfig(const AppConfig &cfg);
  bool isComplete() const { return complete_; }
  bool isSaved() const { return saved_; }
  AppConfig updateConfig(AppConfig cfg) const;

private:
  void recalcLayout();

  FontManager &fontMgr_;

  // Fields: 0=host, 1=port, 2=login
  static constexpr int kNumFields = 3;
  int activeField_ = 0;
  std::string hostText_;
  std::string portText_;
  std::string loginText_;
  bool useWSJTX_ = false;

  int cursorPos_ = 0;
  bool complete_ = false;
  bool saved_ = false;

  SDL_Rect toggleRect_ = {0, 0, 0, 0};
  SDL_Rect saveRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};

  // Layout metrics
  int titleSize_ = 32;
  int labelSize_ = 18;
  int fieldSize_ = 24;
  int hintSize_ = 14;
};
