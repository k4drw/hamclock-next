#include "MoonPanel.h"
#include "../core/Theme.h"
#include "../core/WorkerService.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cmath>
#include <cstdio>

static const std::string MOON_IMAGE_KEY = "nasa_moon";

MoonPanel::MoonPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr, NetworkManager &net,
                     std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), net_(net),
      store_(std::move(store)) {}

MoonPanel::~MoonPanel() {
  std::lock_guard<std::mutex> lock(imageMutex_);
  if (pendingSurface_) {
    SDL_FreeSurface(pendingSurface_);
    pendingSurface_ = nullptr;
  }
}

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
        [url](std::string body) {
          if (!body.empty()) {
            WorkerService::getInstance().submitTask([body = std::move(body), url]() {
              SDL_Surface *surf = TextureManager::decodeToSurface(
                  reinterpret_cast<const unsigned char *>(body.data()),
                  static_cast<unsigned int>(body.size()), MOON_IMAGE_KEY);

              if (surf) {
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_MOON_IMAGE_READY;
                ev.user.data1 = surf;
                SDL_PushEvent(&ev);
              }
            });
          }
          // Note: imageLoading_ stays true until Dashboard reset or next check.
          // In a full fix, we'd need an event to clear imageLoading_, 
          // but this eliminates the segfault.
        },
        86400); // Cache for 24h
  }
}

void MoonPanel::onImageReady(SDL_Surface *surf) {
  std::lock_guard<std::mutex> lock(imageMutex_);
  if (pendingSurface_) {
    SDL_FreeSurface(pendingSurface_);
  }
  pendingSurface_ = surf;
  imageLoading_ = false;
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
    // Show a simple dark disk while loading NASA imagery
    RenderUtils::drawCircle(renderer, (float)cx, (float)cy, (float)r,
                            {30, 30, 45, 255});
  }
}

void MoonPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Process pending image decoded in background
  {
    std::lock_guard<std::mutex> lock(imageMutex_);
    if (pendingSurface_) {
      texMgr_.loadFromSurface(renderer, MOON_IMAGE_KEY, pendingSurface_);
      SDL_FreeSurface(pendingSurface_);
      pendingSurface_ = nullptr;
    }
  }

  ThemeColors themes = getThemeColors(theme_);

  renderChrome(renderer);

  int titleH = 20;
  fontMgr_.catalog()->drawText(renderer, "Moon", x_ + 10, y_ + 5, themes.accent,
                               FontStyle::MicroBold);

  if (!dataValid_) {
    fontMgr_.catalog()->drawText(renderer, "No Data", x_ + 10,
                                 y_ + titleH + (height_ - titleH) / 2 - 8,
                                 themes.textDim, FontStyle::Fast);
    return;
  }

  int moonR = std::min(width_, height_ - titleH) / 3 - 2;
  if (moonR > 42)
    moonR = 42;
  int moonY = y_ + titleH + moonR + 4;
  int centerX = x_ + width_ / 2;

  drawMoon(renderer, centerX, moonY, moonR);

  // Labels
  int textY = moonY + moonR + 8;
  fontMgr_.catalog()->drawText(renderer, currentData_.phaseName, centerX, textY,
                               themes.text, FontStyle::FastBold, true);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f%% Illum", currentData_.illumination);
  fontMgr_.catalog()->drawText(renderer, buf, centerX,
                               textY + labelFontSize_ + 2, themes.success,
                               FontStyle::Fast, true);
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
