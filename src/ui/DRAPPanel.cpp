#include "DRAPPanel.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <SDL.h>
#include <cstdio>
#include <mutex>

static std::mutex drapMutex;
static std::string drapValue;
static bool drapDataReady = false;

DRAPPanel::DRAPPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr, DRAPProvider &provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      provider_(provider) {}

void DRAPPanel::update() {
  uint32_t now = SDL_GetTicks();
  if (now - lastFetch_ > 15 * 60 * 1000 || lastFetch_ == 0) { // 15 mins
    lastFetch_ = now;
    provider_.fetch([](const std::string &data) {
      std::lock_guard<std::mutex> lock(drapMutex);
      drapValue = data;
      drapDataReady = true;
    });
  }
}

void DRAPPanel::render(SDL_Renderer *renderer) {
  // Update current value from provider
  {
    std::lock_guard<std::mutex> lock(drapMutex);
    if (drapDataReady) {
      currentValue_ = drapValue;
      dataReady_ = true;
      drapDataReady = false;
    }
  }

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int titleH = 20;
  // Title
  cat->drawText(renderer, "DRAP", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!dataReady_) {
    cat->drawText(renderer, "Loading DRAP...", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, themes.textDim,
                  FontStyle::Fast, true);
    return;
  }

  // Parse the value to determine color
  float freq = 0.0f;
  if (!currentValue_.empty()) {
    freq = StringUtils::safe_stof(currentValue_);
  }

  // Color coding: higher DRAP = worse conditions
  // < 5 MHz: Green (good)
  // 5-10 MHz: Yellow (moderate)
  // > 10 MHz: Red (poor)
  SDL_Color valueColor;
  if (freq < 5.0f) {
    valueColor = themes.success; // Green
  } else if (freq < 10.0f) {
    valueColor = themes.warning; // Yellow
  } else {
    valueColor = themes.danger; // Red
  }

  // Display the value prominently
  char displayText[32];
  std::snprintf(displayText, sizeof(displayText), "%.1f MHz", freq);

  cat->drawText(renderer, displayText, x_ + width_ / 2, y_ + height_ / 2,
                valueColor, FontStyle::MediumBold, true, false, true);

  // Add description
  cat->drawText(renderer, "Max Frequency", x_ + width_ / 2, y_ + height_ - 20,
                themes.textDim, FontStyle::Micro, true);
}

void DRAPPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}
