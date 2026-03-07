#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <chrono>
#include <vector>

class StopwatchPanel : public Widget {
public:
  StopwatchPanel(int x, int y, int w, int h, FontManager &fontMgr);

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool isConfiguring() const override { return showSetup_; }

  std::string getName() const override { return "Stopwatch"; }
  std::vector<std::string> getActions() const override {
    return {"Toggle", "Lap", "Reset"};
  }
  bool performAction(const std::string &action) override;

  // Lap data access for REST API
  std::chrono::steady_clock::duration elapsed() const;
  bool isRunning() const { return running_; }
  const std::vector<std::chrono::steady_clock::duration> &laps() const {
    return laps_;
  }

private:
  FontManager &fontMgr_;
  bool running_ = false;
  std::chrono::steady_clock::time_point startTime_;
  std::chrono::steady_clock::duration accumulated_ =
      std::chrono::steady_clock::duration::zero();

  // Lap recording
  std::vector<std::chrono::steady_clock::duration> laps_;

  std::string formatTime(std::chrono::steady_clock::duration d) const;
  std::string formatTimeMini(std::chrono::steady_clock::duration d) const;

  // Button rects (set during render for hit-testing)
  SDL_Rect ssRect_ = {};
  SDL_Rect lapRect_ = {};
  SDL_Rect rRect_ = {};

  // Setup State
  bool showSetup_ = false;
  SDL_Rect doneRect_;
  SDL_Rect cancelRect_;

  void renderSetup(SDL_Renderer *renderer);
  bool handleSetupClick(int mx, int my);
};
