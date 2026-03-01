#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"
#include <SDL.h>

class DXClusterSetup : public Widget {
public:
  DXClusterSetup(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;

  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  void setConfig(const AppConfig &cfg);
  bool isComplete() const { return complete_; }
  bool isSaved() const { return saved_; }
  AppConfig updateConfig(AppConfig cfg) const;

private:
  void recalcLayout();
  TextInput &activeInput();

  FontManager &fontMgr_;

  // Fields: 0=host, 1=port, 2=login
  static constexpr int kNumFields = 3;
  int activeField_ = 0;
  TextInput hostInput_;
  TextInput portInput_;
  TextInput loginInput_;
  bool useWSJTX_ = false;

  bool complete_ = false;
  bool saved_ = false;

  SDL_Rect toggleRect_ = {0, 0, 0, 0};
  SDL_Rect saveRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};
};
