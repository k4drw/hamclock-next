#include "PaneContainer.h"
#include "FontCatalog.h"

PaneContainer::PaneContainer(int x, int y, int w, int h, WidgetType initialType,
                             FontManager &fontMgr)
    : Widget(x, y, w, h), currentType_(initialType), fontMgr_(fontMgr) {}

void PaneContainer::setRotation(const std::vector<WidgetType> &types,
                                int intervalS, bool syncRotation) {
  rotation_ = types;
  intervalS_ = intervalS;
  syncRotation_ = syncRotation;
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

void PaneContainer::setPaused(bool paused) {
  paused_ = paused;
  if (!paused_)
    lastRotateMs_ = SDL_GetTicks(); // fresh interval on resume
}

bool PaneContainer::isPaused() const { return paused_; }

void PaneContainer::forceAdvance() {
  if (rotation_.size() < 2)
    return;
  rotationIdx_ = (rotationIdx_ + 1) % rotation_.size();
  currentType_ = rotation_[rotationIdx_];
  if (widgetFactory_) {
    activeWidget_ = widgetFactory_(currentType_);
    if (activeWidget_) {
      activeWidget_->onResize(x_, y_, width_, height_);
      activeWidget_->setTheme(theme_);
    }
  }
  lastRotateMs_ = SDL_GetTicks();
}

void PaneContainer::update() {
  if (activeWidget_) {
    activeWidget_->update();
  }

  if (!paused_ && rotation_.size() > 1 && intervalS_ > 0) {
    Uint32 now = SDL_GetTicks();
    Uint32 intervalMs = static_cast<Uint32>(intervalS_ * 1000);
    bool shouldAdvance = false;
    size_t newIdx = rotationIdx_;

    if (syncRotation_) {
      // Advance at wall-clock epoch boundaries so all panes flip together
      size_t bucket = now / intervalMs;
      size_t lastBucket = lastRotateMs_ / intervalMs;
      if (bucket != lastBucket) {
        newIdx = bucket % rotation_.size();
        shouldAdvance = true;
      }
    } else {
      if (now - lastRotateMs_ >= intervalMs) {
        newIdx = (rotationIdx_ + 1) % rotation_.size();
        shouldAdvance = true;
      }
    }

    if (shouldAdvance) {
      rotationIdx_ = newIdx;
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
  if (width_ <= 0 || height_ <= 0)
    return;
  // Draw content
  if (activeWidget_) {
    activeWidget_->render(renderer);
  } else {
    // Background for empty pane
    SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
    SDL_Rect r = {x_, y_, width_, height_};
    SDL_RenderFillRect(renderer, &r);

    fontMgr_.catalog()->drawText(renderer, widgetTypeDisplayName(currentType_),
                                 x_ + width_ / 2, y_ + height_ / 2,
                                 {100, 100, 120, 255}, FontStyle::UI, true);
  }

  // Draw border
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Draw manual navigation arrows when rotation has multiple widgets
  // Hide if widget is configuring
  if (rotation_.size() > 1 &&
      !(activeWidget_ && activeWidget_->isConfiguring())) {
    int arrowW = std::min(18, width_ / 8);
    int arrowH = std::min(36, height_ / 5);
    int cy = y_ + height_ / 2;
    SDL_Rect lArr = {x_, cy - arrowH / 2, arrowW, arrowH};
    SDL_Rect rArr = {x_ + width_ - arrowW, cy - arrowH / 2, arrowW, arrowH};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
    SDL_RenderFillRect(renderer, &lArr);
    SDL_RenderFillRect(renderer, &rArr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    fontMgr_.catalog()->drawText(renderer, "<", lArr.x + lArr.w / 2,
                                 lArr.y + lArr.h / 2, {220, 220, 220, 255},
                                 FontStyle::Fast, true, false, true);
    fontMgr_.catalog()->drawText(renderer, ">", rArr.x + rArr.w / 2,
                                 rArr.y + rArr.h / 2, {220, 220, 220, 255},
                                 FontStyle::Fast, true, false, true);
  }
}

void PaneContainer::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  if (activeWidget_) {
    activeWidget_->onResize(x, y, w, h);
  }
}

void PaneContainer::onMouseMove(int mx, int my) {
  if (activeWidget_) {
    activeWidget_->onMouseMove(mx, my);
  }
}

bool PaneContainer::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  // 1. If we are acting as a modal proxy, we MUST handle clicks anywhere.
  if (isModalActive() && activeWidget_) {
    if (activeWidget_->onMouseUp(mx, my, mod, clicks)) {
      return true;
    }
    return false;
  }

  // 2. Bounds check
  SDL_Rect r = getRect();
  if (mx < r.x || mx >= r.x + r.w || my < r.y || my >= r.y + r.h) {
    return false;
  }

  // 2a. Check manual navigation arrows (intercept before widget)
  if (rotation_.size() > 1) {
    int arrowW = std::min(18, width_ / 8);
    int arrowH = std::min(36, height_ / 5);
    int cy = y_ + height_ / 2;
    SDL_Rect lArr = {x_, cy - arrowH / 2, arrowW, arrowH};
    SDL_Rect rArr = {x_ + width_ - arrowW, cy - arrowH / 2, arrowW, arrowH};

    if (mx >= lArr.x && mx < lArr.x + lArr.w && my >= lArr.y &&
        my < lArr.y + lArr.h) {
      activateRotationIndex((rotationIdx_ + rotation_.size() - 1) %
                            rotation_.size());
      return true;
    }
    if (mx >= rArr.x && mx < rArr.x + rArr.w && my >= rArr.y &&
        my < rArr.y + rArr.h) {
      activateRotationIndex((rotationIdx_ + 1) % rotation_.size());
      return true;
    }
  }

  // 3. Give active widget first crack at internal elements (header buttons,
  // etc)
  if (activeWidget_ && activeWidget_->onMouseUp(mx, my, mod, clicks)) {
    return true;
  }

  // 4. Pane level logic - top 10% transitions to widget selection
  // If widget is configuring, don't allow pane-level triggers
  if (activeWidget_ && activeWidget_->isConfiguring()) {
    return true;
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
    // If widget didn't handle it, maybe bring up config if clicked in lower 90%
    if (onConfigRequested_) {
      onConfigRequested_(currentType_);
    }
    return true;
  }
}

void PaneContainer::activateRotationIndex(size_t idx) {
  rotationIdx_ = idx;
  currentType_ = rotation_[rotationIdx_];
  if (widgetFactory_) {
    activeWidget_ = widgetFactory_(currentType_);
    if (activeWidget_) {
      activeWidget_->onResize(x_, y_, width_, height_);
      activeWidget_->setTheme(theme_);
    }
  }
  lastRotateMs_ = SDL_GetTicks();
}

bool PaneContainer::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (activeWidget_) {
    return activeWidget_->onKeyDown(key, mod);
  }
  return false;
}

bool PaneContainer::onTextInput(const char *text) {
  if (activeWidget_) {
    return activeWidget_->onTextInput(text);
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
