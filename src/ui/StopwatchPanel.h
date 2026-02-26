#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <chrono>

class StopwatchPanel : public Widget {
public:
  StopwatchPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool isConfiguring() const override { return showSetup_; }

  std::string getName() const override { return "Stopwatch"; }
  std::vector<std::string> getActions() const override {
    return {"Toggle", "Reset"};
  }
  bool performAction(const std::string &action) override;

private:
  FontManager &fontMgr_;
  bool running_ = false;
  std::chrono::steady_clock::time_point startTime_;
  std::chrono::steady_clock::duration accumulated_ =
      std::chrono::steady_clock::duration::zero();

  std::string formatTime(std::chrono::steady_clock::duration d) const;

  // Setup State
  bool showSetup_ = false;
  SDL_Rect doneRect_;
  SDL_Rect cancelRect_;

  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);
};
