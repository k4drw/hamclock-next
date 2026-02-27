#include "ThemeCustomizer.h"
#include "../core/ConfigManager.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <algorithm>

namespace HamClock {

ThemeCustomizer::ThemeCustomizer(int x, int y, int w, int h,
                                 FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {
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
        auto &config = ConfigManager::instance().getConfig();
        config.colorOverrides[colorKeys_[selectedIndex_].key] = c;
        config.theme = "custom"; // Auto-switch to custom theme when editing
      });

  calculateLayout();
}

void ThemeCustomizer::calculateLayout() {
  rectList_ = {x_ + 10, y_ + 40, 200, height_ - 80};
  rectClose_ = {x_ + width_ - 100, y_ + 10, 80, 30};
}

void ThemeCustomizer::setActive(bool active) {
  active_ = active;
  if (active_) {
    // Load current color for selection
    loadOverrides();
  }
}

void ThemeCustomizer::loadOverrides() {
  const auto &config = ConfigManager::instance().getConfig();
  auto it = config.colorOverrides.find(colorKeys_[selectedIndex_].key);
  if (it != config.colorOverrides.end()) {
    colorPicker_->setColor(it->second);
  } else {
    // Fallback to current theme's color
    ThemeColors current = getThemeColors(config.theme);
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

  // Background dimming
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
  SDL_Rect full = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &full);

  // Border
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &full);

  // Title
  // (Assuming FontManager is available via some global or context)
  // For now, I'll just draw the picker.

  // 1. Draw List of Keys
  for (size_t i = 0; i < colorKeys_.size(); ++i) {
    SDL_Rect r = {rectList_.x, rectList_.y + (int)i * 25, rectList_.w, 22};
    if (selectedIndex_ == (int)i) {
      SDL_SetRenderDrawColor(renderer, 60, 60, 90, 255);
      SDL_RenderFillRect(renderer, &r);
    }
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    auto *cat = fontMgr_.catalog();
    cat->drawText(renderer, colorKeys_[i].label.c_str(), rectList_.x + 5,
                  rectList_.y + (int)i * 25 + 2, {255, 255, 255, 255},
                  FontStyle::SmallRegular);
  }

  // 2. Render ColorPicker
  colorPicker_->render(renderer);

  // 3. Close Button
  SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
  SDL_RenderFillRect(renderer, &rectClose_);
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderDrawRect(renderer, &rectClose_);
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "Close", rectClose_.x + rectClose_.w / 2,
                rectClose_.y + 5, {255, 255, 255, 255}, FontStyle::MediumBold,
                true);
}

bool ThemeCustomizer::onMouseDown(int mx, int my, Uint16 mod) {
  if (!active_)
    return false;

  if (mx >= rectClose_.x && mx < rectClose_.x + rectClose_.w &&
      my >= rectClose_.y && my < rectClose_.y + rectClose_.h) {
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

} // namespace HamClock
