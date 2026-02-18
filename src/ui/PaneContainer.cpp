#include "PaneContainer.h"

PaneContainer::PaneContainer(int x, int y, int w, int h, WidgetType initialType,
                             FontManager &fontMgr)
    : Widget(x, y, w, h), currentType_(initialType), fontMgr_(fontMgr) {}

void PaneContainer::setRotation(const std::vector<WidgetType> &types,
                                int intervalS) {
  rotation_ = types;
  intervalS_ = intervalS;
  if (rotationIdx_ >= rotation_.size()) {
    rotationIdx_ = 0;
  }

  if (!rotation_.empty()) {
    currentType_ = rotation_[rotationIdx_];
    if (widgetFactory_) {
      activeWidget_ = widgetFactory_(currentType_);
      if (activeWidget_) {
        activeWidget_->onResize(x_, y_, width_, height_);
        activeWidget_->setTheme(theme_);
      }
    }
  } else {
    activeWidget_ = nullptr;
  }
  lastRotateMs_ = SDL_GetTicks();
}

void PaneContainer::update() {
  if (activeWidget_) {
    activeWidget_->update();
  }

  if (rotation_.size() > 1 && intervalS_ > 0) {
    Uint32 now = SDL_GetTicks();
    if (now - lastRotateMs_ >= static_cast<Uint32>(intervalS_ * 1000)) {
      rotationIdx_ = (rotationIdx_ + 1) % rotation_.size();
      currentType_ = rotation_[rotationIdx_];
      if (widgetFactory_) {
        activeWidget_ = widgetFactory_(currentType_);
        if (activeWidget_) {
          activeWidget_->onResize(x_, y_, width_, height_);
          activeWidget_->setTheme(theme_);
        }
      }
      lastRotateMs_ = now;
    }
  }
}

void PaneContainer::render(SDL_Renderer *renderer) {
  // Draw content
  if (activeWidget_) {
    activeWidget_->render(renderer);
  } else {
    // Background for empty pane
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_Rect r = {x_, y_, width_, height_};
    SDL_RenderFillRect(renderer, &r);

    fontMgr_.drawText(renderer, widgetTypeDisplayName(currentType_),
                      x_ + width_ / 2, y_ + height_ / 2, {100, 100, 120, 255},
                      14, false, true);
  }

  // Draw border
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Draw title area indicator (top 10%) if debug or hovered?
  // For now just handle the click logic.
}

void PaneContainer::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  if (activeWidget_) {
    activeWidget_->onResize(x, y, w, h);
  }
}

bool PaneContainer::onMouseUp(int mx, int my, Uint16 mod) {
  // If we are acting as a modal proxy, we MUST handle clicks anywhere.
  if (isModalActive() && activeWidget_) {
    if (activeWidget_->onMouseUp(mx, my, mod)) {
      return true;
    }
    // If the modal widget didn't handle it, and it's modal, we still don't
    // want to process it as a non-modal click on the pane itself.
    // The instruction implies that if it's modal, the event is processed
    // by the activeWidget_ regardless of bounds. If activeWidget_ returns
    // false, it means it didn't handle it, and we should not then
    // fall through to the pane's own click handling.
    return false; // Modal widget didn't handle it, so no further processing by
                  // PaneContainer
  }

  SDL_Rect r = getRect();
  if (mx < r.x || mx >= r.x + r.w || my < r.y || my >= r.y + r.h) {
    return false;
  }

  int relativeY = my - r.y;
  int titleThreshold = r.h / 10; // Top 10%

  if (relativeY < titleThreshold) {
    // Change widget requested
    if (onSelectionRequested_) {
      onSelectionRequested_(paneIndex_, mx, my);
    }
    return true;
  } else {
    // Config or normal widget interaction
    if (activeWidget_) {
      if (activeWidget_->onMouseUp(mx, my, mod)) {
        return true;
      }
    }

    // If widget didn't handle it, maybe bring up config if clicked in lower 90%
    if (onConfigRequested_) {
      onConfigRequested_(currentType_);
    }
    return true;
  }
}

bool PaneContainer::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (isModalActive() && activeWidget_) {
    return activeWidget_->onKeyDown(key, mod);
  }
  return false;
}

std::vector<std::string> PaneContainer::getActions() const {
  std::vector<std::string> actions = {"change_rotation", "tap", "rotate"};
  if (activeWidget_) {
    for (const auto &a : activeWidget_->getActions()) {
      actions.push_back(a);
    }
  }
  return actions;
}

SDL_Rect PaneContainer::getActionRect(const std::string &action) const {
  if (action == "change_rotation") {
    return {x_, y_, width_, height_ / 10};
  }
  if (action == "tap") {
    return {x_, y_ + height_ / 10, width_, height_ * 9 / 10};
  }
  if (action == "rotate") {
    // Manual rotation could be anywhere, but let's map it to the
    // change_rotation area for simplicity
    return {x_, y_, width_, height_ / 10};
  }

  if (activeWidget_) {
    return activeWidget_->getActionRect(action);
  }

  return {0, 0, 0, 0};
}

nlohmann::json PaneContainer::getDebugData() const {
  if (activeWidget_) {
    return activeWidget_->getDebugData();
  }
  return {};
}
