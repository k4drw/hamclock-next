#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"
#include <chrono>
#include <string>

struct SDL_Renderer;

class CountdownPanel : public Widget {
public:
  CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 AppConfig &config);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

  // Modal overrides
  bool isModalActive() const override { return editing_; }
  void renderModal(SDL_Renderer *renderer) override { renderSetup(renderer); }

private:
  void startEditing();
  void stopEditing(bool apply);
  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);

  FontManager &fontMgr_;
  AppConfig &config_;
  std::chrono::system_clock::time_point targetTime_;

  // Editor state
  bool editing_ = false;
  int activeField_ = 0; // 0=Label, 1=Time
  std::string labelEdit_;
  std::string timeEdit_;
  int cursorPos_ = 0;

  SDL_Rect menuRect_ = {0, 0, 0, 0};
  SDL_Rect labelRect_ = {0, 0, 0, 0};
  SDL_Rect timeRect_ = {0, 0, 0, 0};
  SDL_Rect okRect_ = {0, 0, 0, 0};
  SDL_Rect cancelRect_ = {0, 0, 0, 0};
};
