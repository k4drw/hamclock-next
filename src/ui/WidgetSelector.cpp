#include "WidgetSelector.h"
#include "WidgetRegistry.h"
#include "../core/Constants.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>

WidgetSelector::WidgetSelector(FontManager &fontMgr)
    : Widget(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT),
      fontMgr_(fontMgr) {}

void WidgetSelector::show(
    int paneIndex, const std::vector<std::string> &available,
    const std::vector<std::string> &currentSelection,
    const std::vector<std::string> &forbidden,
    std::function<void(int, const std::vector<std::string> &, bool)> onDone,
    bool singleSelect, bool startFullHeight) {
  singleSelect_ = singleSelect;
  paneIndex_ = paneIndex;
  fullHeightMode_ = startFullHeight;
  
  allAvailable_ = available; // save before full-height filter for toggle use
  available_ = available;
  if (fullHeightMode_) {
    available_.erase(std::remove_if(available_.begin(), available_.end(),
                                    [](const std::string &t) {
                                      auto *d = WidgetRegistry::instance().find(t);
                                      return !d || !d->isScrollable;
                                    }),
                     available_.end());
  }

  // Alphabetize by display name
  std::sort(available_.begin(), available_.end(),
            [](const std::string &a, const std::string &b) {
              auto *da = WidgetRegistry::instance().find(a);
              auto *db = WidgetRegistry::instance().find(b);
              const char *na = da ? da->displayName : a.c_str();
              const char *nb = db ? db->displayName : b.c_str();
              return std::string(na) < std::string(nb);
            });

  selection_ = currentSelection;
  forbidden_ = forbidden;
  onDone_ = onDone;
  visible_ = true;
  focusedIdx_ = 0;

  // Center the menu
  int numCols = 4; // 4 columns to fit ~47 widgets without scrolling
  int itemH = 20;
  int baseW = 180; // 4 × 180 = 720px, fits in 800px logical width
  int menuW = baseW * numCols;
  int footerH = 50;

  int numRows = (static_cast<int>(available_.size()) + numCols - 1) / numCols;
  int menuH =
      numRows * itemH + footerH + 25; // Increased padding to prevent overlap

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
    itemRects_.push_back({menuRect_.x + col * colW,
                          menuRect_.y + row * itemH + 10, colW, itemH});
  }

  // Position footer buttons
  int btnW = 100;
  int btnH = 34;
  int btnY = menuRect_.y + menuRect_.h - btnH - 12;
  cancelRect_ = {menuRect_.x + menuW / 2 - btnW - 10, btnY, btnW, btnH};
  okRect_ = {menuRect_.x + menuW / 2 + 10, btnY, btnW, btnH};

  if (paneIndex_ == 4 || paneIndex_ == 5) {
    fullHeightRect_ = {menuRect_.x + 20, btnY + 8, 110, 20};
  } else {
    fullHeightRect_ = {0, 0, 0, 0};
  }
}

void WidgetSelector::hide() { visible_ = false; }

void WidgetSelector::update() {}

void WidgetSelector::render(SDL_Renderer *renderer) {
  if (!visible_)
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Dim background
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 150);
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

  auto *cat = fontMgr_.catalog();

  // Clip item rendering to the area above the footer separator so items
  // can never visually overflow into the button row if the list is large.
  int itemClipH = okRect_.y - 8 - menuRect_.y;
  SDL_Rect itemClip = {menuRect_.x, menuRect_.y, menuRect_.w, itemClipH};
  SDL_RenderSetClipRect(renderer, &itemClip);

  for (size_t i = 0; i < available_.size(); ++i) {
    const std::string &t = available_[i];
    bool isForbidden =
        std::find(forbidden_.begin(), forbidden_.end(), t) != forbidden_.end();
    bool isSelected =
        std::find(selection_.begin(), selection_.end(), t) != selection_.end();

    // Draw focus indicator BEFORE text so it doesn't cover the text
    if (visible_ && static_cast<int>(i) == focusedIdx_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 80);
      SDL_Rect focusRect = itemRects_[i];
      focusRect.x += 5;
      focusRect.w -= 10;
      SDL_RenderFillRect(renderer, &focusRect);
    }

    SDL_Color textColor = themes.text;
    if (isSelected) {
      textColor = themes.accent; // Selected always takes priority
    } else if (isForbidden) {
      textColor = themes.textDim;
    }

    auto *desc = WidgetRegistry::instance().find(t);
    const char *label = desc ? desc->displayName : t.c_str();
    cat->drawText(renderer, label,
                  itemRects_[i].x + itemRects_[i].w / 2,
                  itemRects_[i].y + itemRects_[i].h / 2, textColor,
                  FontStyle::SmallRegular, true, false, true);

    // Draw separator (if not last row)
    int numCols = 4;
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

  // Restore clip to full menu area before drawing footer
  SDL_RenderSetClipRect(renderer, &menuRect_);

  // Footer separator
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, menuRect_.x + 5, okRect_.y - 8,
                     menuRect_.x + menuRect_.w - 5, okRect_.y - 8);

  if (fullHeightRect_.w > 0) {
    // Checkbox box
    SDL_Rect cb = {fullHeightRect_.x, fullHeightRect_.y, 16, 16};
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &cb);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &cb);

    // Inner tick mark
    if (fullHeightMode_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, 255);
      SDL_Rect inner = {cb.x + 3, cb.y + 3, 10, 10};
      SDL_RenderFillRect(renderer, &inner);
    }

    // Checkbox text
    cat->drawText(renderer, "Full height", cb.x + 22, cb.y + 8,
                  fullHeightMode_ ? themes.text : themes.textDim,
                  FontStyle::Fast, false, false, true);
  }

  // Buttons
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &okRect_);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 100);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  SDL_RenderDrawRect(renderer, &okRect_);

  cat->drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                cancelRect_.y + cancelRect_.h / 2, themes.bg, FontStyle::UI,
                true, false, true);
  cat->drawText(renderer, "Done", okRect_.x + okRect_.w / 2,
                okRect_.y + okRect_.h / 2, themes.bg, FontStyle::UIBold, true,
                false, true);
}

bool WidgetSelector::onMouseUp(int mx, int my, Uint16 /*mod*/, int clicks) {
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
      onDone_(paneIndex_, selection_, fullHeightMode_);
    }
    hide();
    return true;
  }

  // Check full height checkbox
  if (fullHeightRect_.w > 0 && mx >= fullHeightRect_.x &&
      mx < fullHeightRect_.x + fullHeightRect_.w &&
      my >= fullHeightRect_.y && my < fullHeightRect_.y + fullHeightRect_.h) {
    fullHeightMode_ = !fullHeightMode_;

    // Re-filter from the original available list (same scope as initial show())
    std::vector<std::string> newAvailable;
    for (const auto &t : allAvailable_) {
      auto *d = WidgetRegistry::instance().find(t);
      if (!fullHeightMode_ || (d && d->isScrollable)) {
        newAvailable.push_back(t);
      }
    }
    std::sort(newAvailable.begin(), newAvailable.end(),
              [](const std::string &a, const std::string &b) {
                auto *da = WidgetRegistry::instance().find(a);
                auto *db = WidgetRegistry::instance().find(b);
                const char *na = da ? da->displayName : a.c_str();
                const char *nb = db ? db->displayName : b.c_str();
                return std::string(na) < std::string(nb);
              });
    available_ = newAvailable;

    // Prune invalid selections
    auto it = std::remove_if(selection_.begin(), selection_.end(),
                             [&](const std::string &t) {
                               return std::find(available_.begin(),
                                                available_.end(),
                                                t) == available_.end();
                             });
    selection_.erase(it, selection_.end());
    
    // Focus clamps
    if (focusedIdx_ >= static_cast<int>(available_.size())) {
      focusedIdx_ = std::max(0, static_cast<int>(available_.size()) - 1);
    }

    // Re-calculate layout geometry
    int numCols = 4;
    int itemH = 28;
    int baseW = 180;
    int menuW = baseW * numCols;
    int footerH = 50;

    int numRows = (static_cast<int>(available_.size()) + numCols - 1) / numCols;
    int menuH =
        numRows * itemH + footerH + 25; // Increased padding to prevent overlap

    if (menuH > HamClock::LOGICAL_HEIGHT - 20) {
      menuH = HamClock::LOGICAL_HEIGHT - 20;
    }

    menuRect_ = {HamClock::LOGICAL_WIDTH / 2 - menuW / 2,
                 HamClock::LOGICAL_HEIGHT / 2 - menuH / 2, menuW, menuH};

    int btnW = 100;
    int btnH = 34;
    int btnY = menuRect_.y + menuRect_.h - btnH - 12;
    cancelRect_ = {menuRect_.x + menuW / 2 - btnW - 10, btnY, btnW, btnH};
    okRect_ = {menuRect_.x + menuW / 2 + 10, btnY, btnW, btnH};

    if (paneIndex_ == 4 || paneIndex_ == 5) {
      fullHeightRect_ = {menuRect_.x + 20, btnY + 8, 110, 20};
    } else {
      fullHeightRect_ = {0, 0, 0, 0};
    }

    // Re-build itemRects_
    int colW = menuRect_.w / numCols;
    itemRects_.clear();
    for (size_t i = 0; i < available_.size(); ++i) {
      int row = static_cast<int>(i) / numCols;
      int col = static_cast<int>(i) % numCols;
      itemRects_.push_back({menuRect_.x + col * colW,
                            menuRect_.y + row * itemH + 10, colW, itemH});
    }

    return true;
  }

  for (size_t i = 0; i < itemRects_.size(); ++i) {
    if (mx >= itemRects_[i].x && mx < itemRects_[i].x + itemRects_[i].w &&
        my >= itemRects_[i].y && my < itemRects_[i].y + itemRects_[i].h) {
      const std::string &t = available_[i];

      // Note: Forbidden check disabled here to allow selecting duplicates
      // if the user specifically requests them.

      bool isSelected = std::find(selection_.begin(), selection_.end(), t) !=
                        selection_.end();

      // Update local selection for immediate UI feedback
      if (singleSelect_) {
        selection_ = {t}; // radio: replace entire selection
      } else if (isSelected) {
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

  // Click outside menuRect_ closes the menu (Cancel)
  if (mx < menuRect_.x || mx >= menuRect_.x + menuRect_.w || my < menuRect_.y ||
      my >= menuRect_.y + menuRect_.h) {
    hide();
  }
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
      onDone_(paneIndex_, selection_, fullHeightMode_);
    }
    hide();
    return true;
  }

  // Navigation
  int numCols = 4;
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
      const std::string &t = available_[focusedIdx_];
      bool isForbidden = std::find(forbidden_.begin(), forbidden_.end(), t) !=
                         forbidden_.end();
      if (!isForbidden) {
        if (singleSelect_) {
          selection_ = {t};
        } else {
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
    }
    return true;
  }
  return false;
}
