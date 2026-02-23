#pragma once

#include "../services/UpdateChecker.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>
#include <string>
#include <vector>

class UpdateOverlay : public Widget {
public:
  enum class Result { None, Skip, NotNow, Update };

  UpdateOverlay(int x, int y, int w, int h, FontManager &fontMgr,
                UpdateChecker &updateChecker);
  ~UpdateOverlay() override;

  void update() override;
  void render(SDL_Renderer *renderer) override;
  void onResize(int x, int y, int w, int h) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onMouseWheel(int scrollY) override;

  Result getResult() const { return result_; }
  void reset() { result_ = Result::None; }

  std::string getName() const override { return "UpdateOverlay"; }

private:
  void recalcLayout();
  void renderReleaseNotes(SDL_Renderer *renderer, SDL_Rect area);

  FontManager &fontMgr_;
  UpdateChecker &updateChecker_;

  Result result_ = Result::None;

  // Layout
  int titleSize_ = 24;
  int bodySize_ = 14;
  int btnSize_ = 16;
  int padding_ = 20;

  SDL_Rect skipBtn_ = {0, 0, 0, 0};
  SDL_Rect notNowBtn_ = {0, 0, 0, 0};
  SDL_Rect updateBtn_ = {0, 0, 0, 0};
  SDL_Rect notesArea_ = {0, 0, 0, 0};

  // Scrolling
  int scrollPos_ = 0;
  int maxScroll_ = 0;

  struct StyledLine {
    std::string text;
    int size;
    SDL_Color color;
    bool bold;
    int indent;
  };

  // Cached content
  std::vector<StyledLine> wrappedLines_;
  std::string lastNotes_;
  int lastWidth_ = 0;
};
