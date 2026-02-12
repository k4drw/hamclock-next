#include "DRAPPanel.h"
#include "../core/Theme.h"
#include <SDL2/SDL.h>
#include <mutex>

static std::mutex drapMutex;
static std::string drapPendingData;
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
      drapPendingData = data;
      drapDataReady = true;
    });
  }
}

void DRAPPanel::render(SDL_Renderer *renderer) {
  {
    std::lock_guard<std::mutex> lock(drapMutex);
    if (drapDataReady) {
      texMgr_.loadFromMemory(renderer, "drap_latest", drapPendingData);
      drapDataReady = false;
      drapPendingData.clear();
      imageReady_ = true;
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

  SDL_Texture *tex = texMgr_.get("drap_latest");
  if (tex && imageReady_) {
    // DRAP images are world maps (2:1 or similar)
    // Adjust vertically to fit
    int drawW = width_ - 10;
    int drawH = (int)(drawW * 0.5f); // 2:1 ratio
    if (drawH > height_ - 20) {
      drawH = height_ - 20;
      drawW = (int)(drawH * 2.0f);
    }
    SDL_Rect dst = {x_ + (width_ - drawW) / 2, y_ + (height_ - drawH) / 2 + 5,
                    drawW, drawH};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
  } else {
    fontMgr_.drawText(renderer, "Loading DRAP...", x_ + width_ / 2,
                      y_ + height_ / 2, {150, 150, 150, 255}, 12, false, true);
  }

  fontMgr_.drawText(renderer, "DRAP Absorption", x_ + 5, y_ + 5, themes.accent,
                    10);
}
