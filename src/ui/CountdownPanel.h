#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "TextInput.h"
#include "Widget.h"
#include <chrono>
#include <string>

struct SDL_Renderer;

class CountdownPanel : public Widget {
public:
  CountdownPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 AppConfig &config, std::function<void()> onSave);

  std::string getName() const override { return "Countdown"; }
  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;
  bool onTextInput(const char *text) override;

private:
  void startEditing(bool editingTime);
  void stopEditing(bool apply);
  void renderEditOverlay(SDL_Renderer *renderer);

  void onMouseMove(int mx, int my) override;

  FontManager &fontMgr_;
  AppConfig &config_;
  std::function<void()> onSave_;
  std::chrono::system_clock::time_point targetTime_;

  // Editor state
  bool editing_ = false;
  bool editingTime_ = false; // true if editing time, false if editing label
  TextInput editInput_;
  bool alarmTriggered_ = false;

  // Temp storage for Multi-field editing
  std::string tempLabel_;
  std::string tempTime_;
};
