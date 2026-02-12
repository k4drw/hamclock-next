#include "AuroraPanel.h"
#include "../core/Theme.h"
#include <SDL2/SDL.h>
#include <mutex>

static std::mutex auroraMutex;
static std::string auroraPendingData;
static bool auroraDataReady = false;

AuroraPanel::AuroraPanel(int x, int y, int w, int h, FontManager &fontMgr,
                         TextureManager &texMgr, AuroraProvider &provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      provider_(provider) {}

void AuroraPanel::update() {
  uint32_t now = SDL_GetTicks();
  if (now - lastFetch_ > 30 * 60 * 1000 || lastFetch_ == 0) { // 30 mins
    lastFetch_ = now;
    provider_.fetch(north_, [](const std::string &data) {
      std::lock_guard<std::mutex> lock(auroraMutex);
      auroraPendingData = data;
      auroraDataReady = true;
    });
  }
}

void AuroraPanel::render(SDL_Renderer *renderer) {
  {
    std::lock_guard<std::mutex> lock(auroraMutex);
    if (auroraDataReady) {
      texMgr_.loadFromMemory(renderer, "aurora_latest", auroraPendingData);
      auroraDataReady = false;
      auroraPendingData.clear();
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

  SDL_Texture *tex = texMgr_.get("aurora_latest");
  if (tex && imageReady_) {
    // Aurora images are typically square or circular
    int drawSz = std::min(width_, height_) - 10;
    SDL_Rect dst = {x_ + (width_ - drawSz) / 2, y_ + (height_ - drawSz) / 2 + 5,
                    drawSz, drawSz};
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
  } else {
    fontMgr_.drawText(renderer, "Loading Aurora...", x_ + width_ / 2,
                      y_ + height_ / 2, {150, 150, 150, 255}, 12, false, true);
  }

  fontMgr_.drawText(renderer,
                    north_ ? "Aurora Forecast (N)" : "Aurora Forecast (S)",
                    x_ + 5, y_ + 5, themes.accent, 10);
}
