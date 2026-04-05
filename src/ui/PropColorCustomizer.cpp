#include "PropColorCustomizer.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <SDL.h>

namespace HamClock {

PropColorCustomizer::PropColorCustomizer(int x, int y, int w, int h,
                                         FontManager &fontMgr,
                                         const std::string &theme,
                                         std::map<std::string, SDL_Color> &overrides,
                                         std::function<void()> onUpdate)
    : Widget(x, y, w, h), fontMgr_(fontMgr), overrides_(overrides),
      onUpdate_(onUpdate) {
  theme_ = theme;
  colorKeys_ = {{"prop_color_0", "0% (Poor)"},
                {"prop_color_25", "25%"},
                {"prop_color_50", "50% (Fair)"},
                {"prop_color_75", "75%"},
                {"prop_color_100", "100% (Excellent)"}};

  int pickerW = w - 230;
  int pickerH = h - 110;
  colorPicker_ = std::make_unique<ColorPicker>(
      x + 220, y + 40, pickerW, pickerH, [this](SDL_Color c) {
        overrides_[colorKeys_[selectedIndex_].key] = c;
        if (onUpdate_) onUpdate_();
      });

  calculateLayout();
}

void PropColorCustomizer::calculateLayout() {
  rectList_ = {x_ + 10, y_ + 40, 200, height_ - 100};
  int btnW = 100;
  int btnH = 34;
  int bx = x_ + width_ / 2;
  int by = y_ + height_ - btnH - 12;
  rectCancel_ = {bx - btnW - 10, by, btnW, btnH};
  rectOk_ = {bx + 10, by, btnW, btnH};
}

void PropColorCustomizer::setActive(bool active) {
  active_ = active;
  if (active_) {
    // Backup current overrides to allow Cancel
    overridesBackup_ = overrides_;
    loadOverrides();
  }
}

void PropColorCustomizer::loadOverrides() {
  auto it = overrides_.find(colorKeys_[selectedIndex_].key);
  if (it != overrides_.end()) {
    colorPicker_->setColor(it->second);
  }
}

void PropColorCustomizer::update() {
  if (active_)
    colorPicker_->update();
}

void PropColorCustomizer::render(SDL_Renderer *renderer) {
  if (!active_)
    return;

  // Use current theme colors for the UI itself
  // We'll peek into overrides, though usually 'theme' is passed to getThemeColors
  // but we can just use "default" as a fallback for the UI look.
  ThemeColors themes = getThemeColors(theme_, overrides_);
  auto *cat = fontMgr_.catalog();

  // Draw opaque overlay
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &bg);

  // Title
  cat->drawText(renderer, "Custom Propagation Gradient", x_ + 10, y_ + 10,
                themes.accent, FontStyle::UIBold);

  // 1. Draw List of Scale Points
  for (size_t i = 0; i < colorKeys_.size(); ++i) {
    SDL_Rect r = {rectList_.x, rectList_.y + (int)i * 30, rectList_.w, 28};
    SDL_Color c = overrides_[colorKeys_[i].key];

    if (selectedIndex_ == (int)i) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 60);
      SDL_RenderFillRect(renderer, &r);
    }
    
    // Preview swatch
    SDL_Rect swatch = {r.x + 5, r.y + 4, 20, 20};
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(renderer, &swatch);
    SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 100);
    SDL_RenderDrawRect(renderer, &swatch);

    cat->drawText(renderer, colorKeys_[i].label.c_str(), r.x + 35,
                  r.y + 4, themes.text, FontStyle::UI);
  }

  // 2. Render ColorPicker
  colorPicker_->render(renderer);

  // 3. OK and Cancel Buttons
  auto drawBottomBtn = [&](const SDL_Rect &r, const char *label, SDL_Color btnBg) {
    SDL_SetRenderDrawColor(renderer, btnBg.r, btnBg.g, btnBg.b, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 150);
    SDL_RenderDrawRect(renderer, &r);
    cat->drawText(renderer, label, r.x + r.w / 2, r.y + r.h / 2,
                  themes.bg, FontStyle::UIBold, true, false, true);
  };

  drawBottomBtn(rectCancel_, "Cancel", themes.danger);
  drawBottomBtn(rectOk_, "Done", themes.success);
}

bool PropColorCustomizer::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (!active_)
    return false;

  // OK/Done button
  if (mx >= rectOk_.x && mx < rectOk_.x + rectOk_.w && my >= rectOk_.y &&
      my < rectOk_.y + rectOk_.h) {
    setActive(false);
    return true;
  }

  // Cancel button
  if (mx >= rectCancel_.x && mx < rectCancel_.x + rectCancel_.w &&
      my >= rectCancel_.y && my < rectCancel_.y + rectCancel_.h) {
    // Revert and refresh
    overrides_ = overridesBackup_;
    if (onUpdate_) onUpdate_();
    setActive(false);
    return true;
  }

  // List selection
  if (mx >= rectList_.x && mx < rectList_.x + rectList_.w &&
      my >= rectList_.y && my < rectList_.y + rectList_.h) {
    int idx = (my - rectList_.y) / 30;
    if (idx >= 0 && idx < (int)colorKeys_.size()) {
      selectedIndex_ = idx;
      loadOverrides();
      return true;
    }
  }

  return colorPicker_->onMouseDown(mx, my, mod, clicks);
}

bool PropColorCustomizer::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!active_)
    return false;
  return colorPicker_->onMouseUp(mx, my, mod, clicks);
}

void PropColorCustomizer::onMouseMove(int mx, int my) {
  if (!active_)
    return;
  colorPicker_->onMouseMove(mx, my);
}

} // namespace HamClock
