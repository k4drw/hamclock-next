#include "DRAPPanel.h"
#include "../core/Theme.h"
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

  // Title
  fontMgr_.drawText(renderer, "DRAP Absorption", x_ + 5, y_ + 5, themes.accent,
                    10);

  if (!dataReady_) {
    fontMgr_.drawText(renderer, "Loading...", x_ + width_ / 2, y_ + height_ / 2,
                      {150, 150, 150, 255}, 12, false, true);
    return;
  }

  // Parse the value to determine color
  float freq = 0.0f;
  if (!currentValue_.empty()) {
    freq = std::stof(currentValue_);
  }

  // Color coding: higher DRAP = worse conditions
  // < 5 MHz: Green (good)
  // 5-10 MHz: Yellow (moderate)
  // > 10 MHz: Red (poor)
  SDL_Color valueColor;
  if (freq < 5.0f) {
    valueColor = {0, 255, 0, 255}; // Green
  } else if (freq < 10.0f) {
    valueColor = {255, 255, 0, 255}; // Yellow
  } else {
    valueColor = {255, 50, 50, 255}; // Red
  }

  // Display the value prominently
  char displayText[32];
  std::snprintf(displayText, sizeof(displayText), "%.1f MHz", freq);

  int valueFontSize = std::max(16, height_ / 4);
  fontMgr_.drawText(renderer, displayText, x_ + width_ / 2, y_ + height_ / 2,
                    valueColor, valueFontSize, false, true);

  // Add description
  fontMgr_.drawText(renderer, "Max Frequency", x_ + width_ / 2,
                    y_ + height_ - 20, themes.textDim, 10, false, true);
}
