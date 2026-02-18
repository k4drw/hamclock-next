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

private:
  void startEditing(bool editingTime);
  void stopEditing(bool apply);
  void renderEditOverlay(SDL_Renderer *renderer);

  FontManager &fontMgr_;
  AppConfig &config_;
  std::chrono::system_clock::time_point targetTime_;

  // Editor state
  bool editing_ = false;
  bool editingTime_ = false; // true if editing time, false if editing label
  std::string editText_;
  int cursorPos_ = 0;
  bool alarmTriggered_ = false;

  // Temp storage for Multi-field editing
  std::string tempLabel_;
  std::string tempTime_;
};
