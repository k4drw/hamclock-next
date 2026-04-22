#include "PaneContainer.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include "WidgetRegistry.h"

PaneContainer::PaneContainer(int x, int y, int w, int h,
                             const std::string &initialType,
                             FontManager &fontMgr)
    : Widget(x, y, w, h), currentType_(initialType), fontMgr_(fontMgr),
      mouseX_(-1), mouseY_(-1) {}

void PaneContainer::setRotation(const std::vector<std::string> &types,
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
        activeWidget_->setLineAATexture(lineAATex_);
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

void PaneContainer::jumpToType(const std::string &typeId) {
  for (size_t i = 0; i < rotation_.size(); ++i) {
    if (rotation_[i] == typeId) {
      activateRotationIndex(i);
      return;
    }
  }
}

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
      activeWidget_->setLineAATexture(lineAATex_);
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
          activeWidget_->setLineAATexture(lineAATex_);
        }
      }
      lastRotateMs_ = now;
    }
  }
}

void PaneContainer::render(SDL_Renderer *renderer) {
  if (width_ <= 0 || height_ <= 0)
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Draw content
  if (activeWidget_) {
    activeWidget_->render(renderer);
  } else {
    // Background for empty pane
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_Rect r = {x_, y_, width_, height_};
    SDL_RenderFillRect(renderer, &r);

    auto *desc = WidgetRegistry::instance().find(currentType_);
    const char *label = desc ? desc->displayName : currentType_.c_str();
    fontMgr_.catalog()->drawText(renderer, label,
                                 x_ + width_ / 2, y_ + height_ / 2,
                                 themes.textDim, FontStyle::UI, true);
  }

  // Draw border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  // Draw maximize / restore button in top-right corner
  if (!(activeWidget_ && activeWidget_->isConfiguring())) {
    int btnSz = std::min(14, std::min(width_, height_) / 6);
    btnSz = std::max(btnSz, 8);
    int bx = x_ + width_ - btnSz - 2;
    int by = y_ + 2;
    maxBtnRect_ = {bx, by, btnSz, btnSz};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 140);
    SDL_RenderFillRect(renderer, &maxBtnRect_);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    int m = 2;
    SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 255);

    if (!expanded_) {
      // Maximize icon: 4 outward corner L-shapes
      SDL_RenderDrawLine(renderer, bx+m,         by+m+3,       bx+m,         by+m);
      SDL_RenderDrawLine(renderer, bx+m,         by+m,         bx+m+3,       by+m);
      SDL_RenderDrawLine(renderer, bx+btnSz-m-3, by+m,         bx+btnSz-m,   by+m);
      SDL_RenderDrawLine(renderer, bx+btnSz-m,   by+m,         bx+btnSz-m,   by+m+3);
      SDL_RenderDrawLine(renderer, bx+m,         by+btnSz-m-3, bx+m,         by+btnSz-m);
      SDL_RenderDrawLine(renderer, bx+m,         by+btnSz-m,   bx+m+3,       by+btnSz-m);
      SDL_RenderDrawLine(renderer, bx+btnSz-m-3, by+btnSz-m,   bx+btnSz-m,   by+btnSz-m);
      SDL_RenderDrawLine(renderer, bx+btnSz-m,   by+btnSz-m,   bx+btnSz-m,   by+btnSz-m-3);
    } else {
      // Restore icon: AA X using preferred line_aa texture method
      SDL_Color col = {themes.text.r, themes.text.g, themes.text.b, 255};
      float fx1 = (float)(bx + m), fy1 = (float)(by + m);
      float fx2 = (float)(bx + btnSz - m), fy2 = (float)(by + btnSz - m);
      if (lineAATex_) {
        RenderUtils::drawThickLineTextured(renderer, lineAATex_, fx1, fy1, fx2, fy2, 1.5f, col);
        RenderUtils::drawThickLineTextured(renderer, lineAATex_, fx2, fy1, fx1, fy2, 1.5f, col);
      } else {
        RenderUtils::drawThickLine(renderer, fx1, fy1, fx2, fy2, 1.5f, col);
        RenderUtils::drawThickLine(renderer, fx2, fy1, fx1, fy2, 1.5f, col);
      }
    }
  } else {
    maxBtnRect_ = {0, 0, 0, 0};
  }

  // Draw manual navigation arrows when rotation has multiple widgets
  // Hide if widget is configuring. ALSO: Hide unless mouse is near edges.
  if (rotation_.size() > 1 && !(activeWidget_ && activeWidget_->isConfiguring())) {
    int arrowW = std::min(18, width_ / 8);
    int arrowH = std::min(36, height_ / 5);
    int cy = y_ + height_ / 2;
    SDL_Rect lArr = {x_, cy - arrowH / 2, arrowW, arrowH};
    SDL_Rect rArr = {x_ + width_ - arrowW, cy - arrowH / 2, arrowW, arrowH};

    // Determine if we should show arrows based on mouse proximity
    bool showArrows = false;
    SDL_Rect bounds = getRect();
    if (mouseX_ >= bounds.x && mouseX_ < bounds.x + bounds.w &&
        mouseY_ >= bounds.y && mouseY_ < bounds.y + bounds.h) {
      // Mouse is in bounds. Show if near left or right.
      int hoverThreshold = arrowW * 2;
      if (mouseX_ < bounds.x + hoverThreshold ||
          mouseX_ >= bounds.x + bounds.w - hoverThreshold) {
        showArrows = true;
      }
    }

    if (showArrows) {
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 140);
      SDL_RenderFillRect(renderer, &lArr);
      SDL_RenderFillRect(renderer, &rArr);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

      fontMgr_.catalog()->drawText(renderer, "<", lArr.x + lArr.w / 2,
                                   lArr.y + lArr.h / 2, themes.text,
                                   FontStyle::Fast, true, false, true);
      fontMgr_.catalog()->drawText(renderer, ">", rArr.x + rArr.w / 2,
                                   rArr.y + rArr.h / 2, themes.text,
                                   FontStyle::Fast, true, false, true);
    }
  }
}

void PaneContainer::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  if (activeWidget_) {
    activeWidget_->onResize(x, y, w, h);
  }
}

void PaneContainer::onMouseMove(int mx, int my) {
  mouseX_ = mx;
  mouseY_ = my;
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

  // 2b. Check maximize/restore button
  if (maxBtnRect_.w > 0) {
    if (mx >= maxBtnRect_.x && mx < maxBtnRect_.x + maxBtnRect_.w &&
        my >= maxBtnRect_.y && my < maxBtnRect_.y + maxBtnRect_.h) {
      if (onMaximizeRequested_) {
        onMaximizeRequested_(paneIndex_);
      }
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
      activeWidget_->setLineAATexture(lineAATex_);
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

bool PaneContainer::onMouseWheel(int scrollY) {
  if (activeWidget_) {
    return activeWidget_->onMouseWheel(scrollY);
  }
  return false;
}

std::vector<std::string> PaneContainer::getActions() const {
  std::vector<std::string> actions = {"select widget"};
  if (!(activeWidget_ && activeWidget_->isConfiguring())) {
    actions.push_back(expanded_ ? "restore" : "maximize");
    if (rotation_.size() > 1) {
      actions.push_back("prev pane");
      actions.push_back("next pane");
    }
  }
  if (activeWidget_) {
    for (const auto &a : activeWidget_->getActions()) {
      actions.push_back(a);
    }
  }
  return actions;
}

std::string PaneContainer::getDisplayName() const {
  if (activeWidget_) {
    std::string name = activeWidget_->getDisplayName();
    if (!name.empty())
      return name;
  }
  auto *desc = WidgetRegistry::instance().find(currentType_);
  return desc ? std::string(desc->displayName) : currentType_;
}

SDL_Rect PaneContainer::getActionRect(const std::string &action) const {
  if (action == "select widget") {
    return {x_, y_, width_, height_ / 10};
  }
  if (action == "maximize" || action == "restore") {
    int btnSz = std::min(14, std::min(width_, height_) / 6);
    btnSz = std::max(btnSz, 8);
    return {x_ + width_ - btnSz - 2, y_ + 2, btnSz, btnSz};
  }
  if (action == "prev pane") {
    int arrowW = std::min(18, width_ / 8);
    int arrowH = std::min(36, height_ / 5);
    int cy = y_ + height_ / 2;
    return {x_, cy - arrowH / 2, arrowW, arrowH};
  }
  if (action == "next pane") {
    int arrowW = std::min(18, width_ / 8);
    int arrowH = std::min(36, height_ / 5);
    int cy = y_ + height_ / 2;
    return {x_ + width_ - arrowW, cy - arrowH / 2, arrowW, arrowH};
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
