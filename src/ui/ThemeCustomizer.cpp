#include "ThemeCustomizer.h"
#include "../core/ConfigManager.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>

namespace HamClock {

ThemeCustomizer::ThemeCustomizer(int x, int y, int w, int h,
                                 FontManager &fontMgr, std::string &theme,
                                 std::map<std::string, SDL_Color> &overrides)
    : Widget(x, y, w, h), fontMgr_(fontMgr), theme_(theme),
      overrides_(overrides) {
  colorKeys_ = {{"bg", "Background"},
                {"border", "Border"},
                {"text", "Text"},
                {"textDim", "Dimmed Text"},
                {"accent", "Accent Color"},
                {"rowStripe1", "Row Stripe 1"},
                {"rowStripe2", "Row Stripe 2"},
                {"success", "Success"},
                {"warning", "Warning"},
                {"danger", "Danger"},
                {"info", "Info"}};

  int pickerSize = std::min(w - 250, h - 100);
  colorPicker_ = std::make_unique<ColorPicker>(
      x + 220, y + 40, pickerSize + 100, pickerSize, [this](SDL_Color c) {
        overrides_[colorKeys_[selectedIndex_].key] = c;
        theme_ = "custom"; // Auto-switch to custom theme when editing
      });

  calculateLayout();
}

void ThemeCustomizer::calculateLayout() {
  rectList_ = {x_ + 10, y_ + 40, 200, height_ - 100};
  int btnW = 100;
  int btnH = 34;
  int bx = x_ + width_ / 2;
  int by = y_ + height_ - btnH - 12;
  rectCancel_ = {bx - btnW - 10, by, btnW, btnH};
  rectOk_ = {bx + 10, by, btnW, btnH};
}

void ThemeCustomizer::setActive(bool active) {
  active_ = active;
  if (active_) {
    // Backup current overrides to allow Cancel
    overridesBackup_ = overrides_;
    themeBackup_ = theme_;
    loadOverrides();
  }
}

void ThemeCustomizer::loadOverrides() {
  auto it = overrides_.find(colorKeys_[selectedIndex_].key);
  if (it != overrides_.end()) {
    colorPicker_->setColor(it->second);
  } else {
    // Fallback to current theme's color
    ThemeColors current = getThemeColors(theme_, &overrides_);
    SDL_Color c = {0, 0, 0, 255};
    std::string k = colorKeys_[selectedIndex_].key;
    if (k == "bg")
      c = current.bg;
    else if (k == "border")
      c = current.border;
    else if (k == "text")
      c = current.text;
    else if (k == "textDim")
      c = current.textDim;
    else if (k == "accent")
      c = current.accent;
    else if (k == "rowStripe1")
      c = current.rowStripe1;
    else if (k == "rowStripe2")
      c = current.rowStripe2;
    else if (k == "success")
      c = current.success;
    else if (k == "warning")
      c = current.warning;
    else if (k == "danger")
      c = current.danger;
    else if (k == "info")
      c = current.info;
    colorPicker_->setColor(c);
  }
}

void ThemeCustomizer::update() {
  if (active_)
    colorPicker_->update();
}

void ThemeCustomizer::render(SDL_Renderer *renderer) {
  if (!active_)
    return;

  ThemeColors themes = getThemeColors(theme_, &overrides_);
  auto *cat = fontMgr_.catalog();

  // Draw background semi-transparent
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 20, 20, 25, 230);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
  SDL_RenderDrawRect(renderer, &bg);

  // 1. Draw List of Keys
  for (size_t i = 0; i < colorKeys_.size(); ++i) {
    SDL_Rect r = {rectList_.x, rectList_.y + (int)i * 25, rectList_.w, 22};
    if (selectedIndex_ == (int)i) {
      SDL_SetRenderDrawColor(renderer, 60, 60, 90, 255);
      SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    cat->drawText(renderer, colorKeys_[i].label.c_str(), rectList_.x + 5,
                  rectList_.y + (int)i * 25 + 2, {255, 255, 255, 255},
                  FontStyle::SmallRegular);
  }

  // 2. Render ColorPicker
  colorPicker_->render(renderer);

  // 3. OK and Cancel Buttons
  auto drawBottomBtn = [&](const SDL_Rect &r, const char *label, SDL_Color bg) {
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2,
                  {255, 255, 255, 255}, FontStyle::UIBold, true, false, true);
  };

  drawBottomBtn(rectCancel_, "Cancel", themes.danger);
  drawBottomBtn(rectOk_, "Done", themes.success);
}

bool ThemeCustomizer::onMouseDown(int mx, int my, Uint16 mod) {
  if (!active_)
    return false;

  if (mx >= rectOk_.x && mx < rectOk_.x + rectOk_.w && my >= rectOk_.y &&
      my < rectOk_.y + rectOk_.h) {
    setActive(false);
    return true;
  }

  if (mx >= rectCancel_.x && mx < rectCancel_.x + rectCancel_.w &&
      my >= rectCancel_.y && my < rectCancel_.y + rectCancel_.h) {
    // Revert overrides and theme
    overrides_ = overridesBackup_;
    theme_ = themeBackup_;
    setActive(false);
    return true;
  }

  if (mx >= rectList_.x && mx < rectList_.x + rectList_.w &&
      my >= rectList_.y && my < rectList_.y + rectList_.h) {
    int idx = (my - rectList_.y) / 25;
    if (idx >= 0 && idx < (int)colorKeys_.size()) {
      selectedIndex_ = idx;
      loadOverrides();
      return true;
    }
  }

  return colorPicker_->onMouseDown(mx, my, mod);
}

bool ThemeCustomizer::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!active_)
    return false;
  return colorPicker_->onMouseUp(mx, my, mod, clicks);
}

void ThemeCustomizer::onMouseMove(int mx, int my) {
  if (!active_)
    return;
  colorPicker_->onMouseMove(mx, my);
}

void ThemeCustomizer::saveOverrides() {
  // Now handled by SetupScreen when clicking Done
}

} // namespace HamClock
