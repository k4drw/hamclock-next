#include "MoonPanel.h"
#include "FontCatalog.h"
#include <cmath>
#include <cstdio>

static const std::string MOON_IMAGE_KEY = "nasa_moon";

MoonPanel::MoonPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr, NetworkManager &net,
                     std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), net_(net),
      store_(std::move(store)) {}

void MoonPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;

  if (dataValid_ && !currentData_.imageUrl.empty() &&
      currentData_.imageUrl != lastImageUrl_ && !imageLoading_) {

    std::string url = currentData_.imageUrl;
    lastImageUrl_ = url;
    imageLoading_ = true;

    net_.fetchAsync(
        url,
        [this](std::string body) {
          if (!body.empty()) {
            // Mark image as ready for texture manager (deferred to render
            // thread) Actually we can't call SDL from here, but
            // TextureManager::loadFromMemory is usually called from render
            // thread. We'll store the bytes and load it in render()
            std::lock_guard<std::mutex> lock(imageMutex_);
            pendingImageData_ = body;
          }
          imageLoading_ = false;
        },
        86400); // Cache for 24h
  }
}

void MoonPanel::drawMoon(SDL_Renderer *renderer, int cx, int cy, int r) {
  SDL_Texture *tex = texMgr_.get(MOON_IMAGE_KEY);
  if (tex) {
    SDL_Rect dst = {cx - r, cy - r, 2 * r, 2 * r};

    // "Flip it for north up": Dial-a-Moon is usually upright,
    // but the user might want explicitly flipped or rotated based on posangle.
    // NASA's posangle is rotation from celestial north.
    // For now, let's just do a 180 flip if requested?
    // Actually, Dial-a-Moon 'su_image' is 'south up'. 'image' is 'north up'.
    // We used 'image', which is north up.

    double angle = currentData_.posangle;
    if (!std::isfinite(angle))
      angle = 0.0;
    SDL_RenderCopyEx(renderer, tex, nullptr, &dst, -angle, nullptr,
                     SDL_FLIP_NONE);
  } else {
    // Fallback procedural
    SDL_SetRenderDrawColor(renderer, 30, 30, 45, 255);
    for (int dy = -r; dy <= r; ++dy) {
      int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
      SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
    SDL_SetRenderDrawColor(renderer, 180, 180, 150, 255);
    double s = 2.0 * currentData_.phase;
    for (int dy = -r; dy <= r; ++dy) {
      double dx = std::sqrt(r * r - dy * dy);
      if (s <= 1.0) {
        double term = (1.0 - 2.0 * s) * dx;
        SDL_RenderDrawLine(renderer, cx + (int)term, cy + dy, cx + (int)dx,
                           cy + dy);
      } else {
        double term = (3.0 - 2.0 * s) * dx;
        SDL_RenderDrawLine(renderer, cx - (int)dx, cy + dy, cx + (int)term,
                           cy + dy);
      }
    }
  }
}

void MoonPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Process pending image
  {
    std::lock_guard<std::mutex> lock(imageMutex_);
    if (!pendingImageData_.empty()) {
      texMgr_.loadFromMemory(renderer, MOON_IMAGE_KEY, pendingImageData_);
      pendingImageData_.clear();
    }
  }

  // Background
  SDL_SetRenderDrawColor(renderer, 10, 10, 15, 255);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "No Data", x_ + 10, y_ + height_ / 2 - 8,
                      {100, 100, 100, 255}, valueFontSize_);
    return;
  }

  int moonR = std::min(width_, height_) / 3 - 2;
  if (moonR > 42)
    moonR = 42;
  int moonY = y_ + moonR + 8;
  int centerX = x_ + width_ / 2;

  drawMoon(renderer, centerX, moonY, moonR);

  // Labels
  int textY = moonY + moonR + 8;
  fontMgr_.drawText(renderer, currentData_.phaseName, centerX, textY,
                    {255, 255, 255, 255}, labelFontSize_, true, true);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f%% Illum", currentData_.illumination);
  fontMgr_.drawText(renderer, buf, centerX, textY + labelFontSize_ + 2,
                    {0, 255, 128, 255}, valueFontSize_, false, true);
}

void MoonPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  valueFontSize_ = cat->ptSize(FontStyle::Fast);
  if (h > 120) {
    labelFontSize_ = cat->ptSize(FontStyle::SmallBold);
    valueFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}
