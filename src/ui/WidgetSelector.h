#pragma once

#include "../core/WidgetType.h"
#include "FontManager.h"
#include "Widget.h"
#include <SDL.h>
#include <functional>
#include <vector>

class WidgetSelector : public Widget {
public:
  WidgetSelector(FontManager &fontMgr);

  void show(int paneIndex, const std::vector<WidgetType> &available,
            const std::vector<WidgetType> &currentSelection,
            const std::vector<WidgetType> &forbidden,
            std::function<void(int, const std::vector<WidgetType> &)> onDone);
  void hide();
  bool isVisible() const { return visible_; }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

  bool isModalActive() const override { return visible_; }
  void renderModal(SDL_Renderer *renderer) override { render(renderer); }

private:
  FontManager &fontMgr_;
  bool visible_ = false;
  int paneIndex_ = 0;
  std::vector<WidgetType> available_;
  std::vector<WidgetType> selection_;
  std::vector<WidgetType> forbidden_;
  std::function<void(int, const std::vector<WidgetType> &)> onDone_;

  SDL_Rect menuRect_;
  std::vector<SDL_Rect> itemRects_;
  SDL_Rect okRect_;
  SDL_Rect cancelRect_;
  int focusedIdx_ = 0;
};
