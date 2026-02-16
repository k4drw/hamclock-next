#include "WidgetSelector.h"
#include "../core/Constants.h"
#include "../core/Theme.h"
#include <algorithm>

WidgetSelector::WidgetSelector(FontManager &fontMgr)
    : Widget(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT),
      fontMgr_(fontMgr) {}

void WidgetSelector::show(
    int paneIndex, const std::vector<WidgetType> &available,
    const std::vector<WidgetType> &currentSelection,
    const std::vector<WidgetType> &forbidden,
    std::function<void(int, const std::vector<WidgetType> &)> onDone) {
  paneIndex_ = paneIndex;
  available_ = available;
  selection_ = currentSelection;
  forbidden_ = forbidden;
  onDone_ = onDone;
  visible_ = true;
  focusedIdx_ = 0;

  // Center the menu
  int numCols = 3; // Use 3 columns to handle more widgets
  int itemH = 34;
  int baseW = 180; // Narrower columns for 3-column layout
  int menuW = baseW * numCols;
  int footerH = 50;

  int numRows = (static_cast<int>(available_.size()) + numCols - 1) / numCols;
  int menuH = numRows * itemH + footerH + 10;

  // Max height clamp to prevent overflowing screen
  if (menuH > HamClock::LOGICAL_HEIGHT - 20) {
    menuH = HamClock::LOGICAL_HEIGHT - 20;
  }

  menuRect_ = {HamClock::LOGICAL_WIDTH / 2 - menuW / 2,
               HamClock::LOGICAL_HEIGHT / 2 - menuH / 2, menuW, menuH};

  itemRects_.clear();
  int colW = menuW / numCols;
  for (size_t i = 0; i < available_.size(); ++i) {
    int row = static_cast<int>(i) / numCols;
    int col = static_cast<int>(i) % numCols;
    itemRects_.push_back(
        {menuRect_.x + col * colW, menuRect_.y + row * itemH + 5, colW, itemH});
  }

  // Position footer buttons
  int btnW = 100;
  int btnH = 34;
  int btnY = menuRect_.y + menuRect_.h - btnH - 10;
  cancelRect_ = {menuRect_.x + menuW / 2 - btnW - 10, btnY, btnW, btnH};
  okRect_ = {menuRect_.x + menuW / 2 + 10, btnY, btnW, btnH};
}

void WidgetSelector::hide() { visible_ = false; }

void WidgetSelector::update() {}

void WidgetSelector::render(SDL_Renderer *renderer) {
  if (!visible_)
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Dim background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
  SDL_Rect screen = {0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT};
  SDL_RenderFillRect(renderer, &screen);

  // Menu background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_RenderFillRect(renderer, &menuRect_);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &menuRect_);

  for (size_t i = 0; i < available_.size(); ++i) {
    WidgetType t = available_[i];
    bool isForbidden =
        std::find(forbidden_.begin(), forbidden_.end(), t) != forbidden_.end();
    bool isSelected =
        std::find(selection_.begin(), selection_.end(), t) != selection_.end();

    // Draw focus indicator BEFORE text so it doesn't cover the text
    if (visible_ && static_cast<int>(i) == focusedIdx_) {
      SDL_SetRenderDrawColor(renderer, 0, 150, 255, 100);
      SDL_Rect focusRect = itemRects_[i];
      focusRect.x += 5;
      focusRect.w -= 10;
      SDL_RenderFillRect(renderer, &focusRect);
    }

    SDL_Color textColor = themes.text;
    if (isForbidden) {
      textColor = {80, 80, 90, 255};
    } else if (isSelected) {
      textColor = themes.accent; // Themed accent for selected
    }

    fontMgr_.drawText(renderer, widgetTypeDisplayName(t),
                      itemRects_[i].x + itemRects_[i].w / 2,
                      itemRects_[i].y + itemRects_[i].h / 2, textColor, 18,
                      false, true); // bold=false, centered=true

    // Draw separator (if not last row)
    int numCols = 3;
    int row = static_cast<int>(i) / numCols;
    int numRows = (static_cast<int>(available_.size()) + numCols - 1) / numCols;
    if (row < numRows - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                             themes.border.b, themes.border.a / 2);
      SDL_RenderDrawLine(renderer, itemRects_[i].x + 10,
                         itemRects_[i].y + itemRects_[i].h,
                         itemRects_[i].x + itemRects_[i].w - 10,
                         itemRects_[i].y + itemRects_[i].h);
    }
  }

  // Footer separator
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, menuRect_.x + 5, okRect_.y - 8,
                     menuRect_.x + menuRect_.w - 5, okRect_.y - 8);

  // Buttons
  SDL_SetRenderDrawColor(renderer, 100, 40, 40, themes.bg.a);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 40, 100, 40, themes.bg.a);
  SDL_RenderFillRect(renderer, &okRect_);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  SDL_RenderDrawRect(renderer, &okRect_);

  fontMgr_.drawText(renderer, "CANCEL", cancelRect_.x + cancelRect_.w / 2,
                    cancelRect_.y + cancelRect_.h / 2, themes.text, 14, false,
                    true);
  fontMgr_.drawText(renderer, "OK", okRect_.x + okRect_.w / 2,
                    okRect_.y + okRect_.h / 2, themes.accent, 14, true, true);
}

bool WidgetSelector::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (!visible_)
    return false;

  // Check footer buttons
  if (mx >= cancelRect_.x && mx < cancelRect_.x + cancelRect_.w &&
      my >= cancelRect_.y && my < cancelRect_.y + cancelRect_.h) {
    hide();
    return true;
  }
  if (mx >= okRect_.x && mx < okRect_.x + okRect_.w && my >= okRect_.y &&
      my < okRect_.y + okRect_.h) {
    if (onDone_) {
      onDone_(paneIndex_, selection_);
    }
    hide();
    return true;
  }

  for (size_t i = 0; i < itemRects_.size(); ++i) {
    if (mx >= itemRects_[i].x && mx < itemRects_[i].x + itemRects_[i].w &&
        my >= itemRects_[i].y && my < itemRects_[i].y + itemRects_[i].h) {
      WidgetType t = available_[i];

      bool isForbidden = std::find(forbidden_.begin(), forbidden_.end(), t) !=
                         forbidden_.end();
      if (isForbidden)
        return true;

      bool isSelected = std::find(selection_.begin(), selection_.end(), t) !=
                        selection_.end();

      // Update local selection for immediate UI feedback
      if (isSelected) {
        if (selection_.size() > 1) {
          selection_.erase(std::remove(selection_.begin(), selection_.end(), t),
                           selection_.end());
        }
      } else {
        selection_.push_back(t);
      }

      return true;
    }
  }

  // Click outside closes the menu (Cancel)
  hide();
  return true;
}

bool WidgetSelector::onKeyDown(SDL_Keycode key, Uint16 /*mod*/) {
  if (!visible_)
    return false;
  if (key == SDLK_ESCAPE) {
    hide();
    return true;
  }
  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    if (onDone_) {
      onDone_(paneIndex_, selection_);
    }
    hide();
    return true;
  }

  // Navigation
  int numCols = 3;
  if (key == SDLK_UP) {
    if (focusedIdx_ >= numCols)
      focusedIdx_ -= numCols;
    return true;
  }
  if (key == SDLK_DOWN) {
    if (focusedIdx_ + numCols < static_cast<int>(available_.size()))
      focusedIdx_ += numCols;
    return true;
  }
  if (key == SDLK_LEFT) {
    if (numCols > 1 && (focusedIdx_ % numCols) > 0)
      --focusedIdx_;
    return true;
  }
  if (key == SDLK_RIGHT) {
    if (numCols > 1 && (focusedIdx_ % numCols) < numCols - 1 &&
        focusedIdx_ + 1 < static_cast<int>(available_.size()))
      ++focusedIdx_;
    return true;
  }
  if (key == SDLK_HOME) {
    focusedIdx_ = 0;
    return true;
  }
  if (key == SDLK_END) {
    focusedIdx_ = static_cast<int>(available_.size()) - 1;
    return true;
  }
  if (key == SDLK_SPACE) {
    if (focusedIdx_ >= 0 && focusedIdx_ < static_cast<int>(available_.size())) {
      WidgetType t = available_[focusedIdx_];
      bool isForbidden = std::find(forbidden_.begin(), forbidden_.end(), t) !=
                         forbidden_.end();
      if (!isForbidden) {
        auto it = std::find(selection_.begin(), selection_.end(), t);
        if (it != selection_.end()) {
          if (selection_.size() > 1) {
            selection_.erase(it);
          }
        } else {
          selection_.push_back(t);
        }
      }
    }
    return true;
  }
  return false;
}
