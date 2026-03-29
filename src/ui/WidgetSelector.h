#pragma once

#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <functional>
#include <string>
#include <vector>

class WidgetSelector : public Widget {
public:
  WidgetSelector(FontManager &fontMgr);

  void show(int paneIndex, const std::vector<std::string> &available,
            const std::vector<std::string> &currentSelection,
            const std::vector<std::string> &forbidden,
            std::function<void(int, const std::vector<std::string> &)> onDone,
            bool singleSelect = false);
  void hide();
  bool isVisible() const { return visible_; }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

  bool isModalActive() const override { return visible_; }
  void renderModal(SDL_Renderer *renderer) override { render(renderer); }

private:
  FontManager &fontMgr_;
  bool visible_ = false;
  int paneIndex_ = 0;
  std::vector<std::string> available_;
  std::vector<std::string> selection_;
  std::vector<std::string> forbidden_;
  std::function<void(int, const std::vector<std::string> &)> onDone_;
  bool singleSelect_ = false;

  SDL_Rect menuRect_;
  std::vector<SDL_Rect> itemRects_;
  SDL_Rect okRect_;
  SDL_Rect cancelRect_;
  int focusedIdx_ = 0;
};
