#include "BeaconPanel.h"
#include "../core/BeaconData.h"
#include "../core/Logger.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cstdio>

BeaconPanel::BeaconPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

BeaconPanel::~BeaconPanel() { clearTextCache(); }

void BeaconPanel::clearTextCache() {
  for (auto &[key, cached] : textCache_) {
    if (cached.texture) {
      MemoryMonitor::getInstance().destroyTexture(cached.texture);
    }
  }
  textCache_.clear();
}

BeaconPanel::CachedText &BeaconPanel::getCachedText(SDL_Renderer *renderer,
                                                    const std::string &key,
                                                    const std::string &text,
                                                    SDL_Color color,
                                                    int fontSize, bool bold) {
  // Check if already cached
  auto it = textCache_.find(key);
  if (it != textCache_.end() && it->second.texture) {
    return it->second;
  }

  // Create new texture
  CachedText &cached = textCache_[key];
  cached.texture = fontMgr_.renderText(renderer, text, color, fontSize,
                                       &cached.w, &cached.h, bold);

  if (!cached.texture) {
    LOG_E("BeaconPanel", "Failed to create cached texture for: {}", text);
  }

  return cached;
}

void BeaconPanel::update() {
  active_ = provider_.getActiveBeacons();
  progress_ = provider_.getSlotProgress();

  // Debug logging for slot changes
  int currentSlot = provider_.getCurrentSlot();
  if (currentSlot != lastSlot_) {
    auto debugInfo = provider_.getDebugInfo();
    LOG_D("BeaconPanel", "Slot {}: Active beacons: {}", currentSlot,
          debugInfo["active_beacons"].dump());
    lastSlot_ = currentSlot;
  }
}

void BeaconPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  bool isNarrow = (width_ < 100);

  if (isNarrow) {
    // Narrow Layout (Fidelity Mode style)
    int pad = 4;
    int centerX = x_ + width_ / 2;
    int curY = y_ + pad;

    // Title - Use cached texture
    char titleKey[64];
    snprintf(titleKey, sizeof(titleKey), "NCDXF_%d_%d_%d_%d_1", themes.text.r,
             themes.text.g, themes.text.b, labelFontSize_);
    auto &titleCache = getCachedText(renderer, titleKey, "NCDXF", themes.text,
                                     labelFontSize_, true);
    if (titleCache.texture) {
      SDL_Rect titleDst = {centerX - titleCache.w / 2,
                           curY + labelFontSize_ / 2 - titleCache.h / 2,
                           titleCache.w, titleCache.h};
      SDL_RenderCopy(renderer, titleCache.texture, nullptr, &titleDst);
    }
    curY += labelFontSize_ + 4;

    // Band colors (approx based on screenshot)
    SDL_Color bandColors[] = {
        {255, 255, 0, 255},   // 20m: Yellow
        {150, 255, 0, 255},   // 17m: Green
        {0, 255, 200, 255},   // 15m: Cyan
        {0, 150, 255, 255},   // 12m: Blue
        {255, 180, 200, 255}, // 10m: Pink
    };
    const char *freqs[] = {"14.10", "18.11", "21.15", "24.93", "28.20"};

    int availableH = (height_ - curY - 6);
    int rowH = availableH / 5;

    for (int i = 0; i < 5; ++i) {
      int ry = curY + i * rowH;
      int iconX = x_ + 10;
      int iconY = ry + rowH / 2;
      int triSize = 6; // Fixed small size like original

      // Draw indicator (Triangle)
      SDL_Color c = bandColors[i];
      RenderUtils::drawTriangle(
          renderer, (float)iconX - triSize, (float)iconY + triSize * 0.5f,
          (float)iconX + triSize, (float)iconY + triSize * 0.5f, (float)iconX,
          (float)iconY - triSize * 0.5f, c);

      // Frequency - Use cached texture
      char freqKey[64];
      snprintf(freqKey, sizeof(freqKey), "%s_%d_%d_%d_%d_0", freqs[i],
               bandColors[i].r, bandColors[i].g, bandColors[i].b,
               callfontSize_);
      auto &freqCache = getCachedText(renderer, freqKey, freqs[i],
                                      bandColors[i], callfontSize_, false);
      if (freqCache.texture) {
        SDL_Rect freqDst = {x_ + 20, ry + rowH / 2 - freqCache.h / 2,
                            freqCache.w, freqCache.h};
        SDL_RenderCopy(renderer, freqCache.texture, nullptr, &freqDst);
      }
    }

    // Progress bar at the bottom
    int barH = 2;
    SDL_Rect progRect = {x_ + 2, y_ + height_ - barH - 2,
                         (int)((width_ - 4) * progress_), barH};
    SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
    SDL_RenderFillRect(renderer, &progRect);
    return;
  }

  // Original Wide Layout (if ever used)
  int pad = 4;
  int callWidth = (width_ > 150) ? 60 : 45;
  int bandWidth = (width_ - callWidth - 2 * pad) / 5;

  int rowHeight = (height_ - 2 * pad - labelFontSize_) / 18;
  if (rowHeight < 2)
    rowHeight = 2;

  // Headers - Use cached textures
  int curX = x_ + pad + callWidth;
  const char *bands[] = {"20", "17", "15", "12", "10"};
  for (int i = 0; i < 5; ++i) {
    char bandKey[64];
    snprintf(bandKey, sizeof(bandKey), "band_%s_%d_%d_%d_%d_0", bands[i],
             themes.textDim.r, themes.textDim.g, themes.textDim.b,
             labelFontSize_);
    auto &bandCache = getCachedText(renderer, bandKey, bands[i], themes.textDim,
                                    labelFontSize_, false);
    if (bandCache.texture) {
      SDL_Rect bandDst = {curX + bandWidth / 2 - bandCache.w / 2,
                          y_ + pad - bandCache.h / 2, bandCache.w, bandCache.h};
      SDL_RenderCopy(renderer, bandCache.texture, nullptr, &bandDst);
    }
    curX += bandWidth;
  }

  // Rows - Use cached textures for callsigns
  int startY = y_ + pad + labelFontSize_ + 2;
  for (int i = 0; i < 18; ++i) {
    int rowY = startY + i * rowHeight;

    // Cache beacon callsign
    char callKey[128];
    const std::string &callsign = NCDXF_BEACONS[i].callsign;
    snprintf(callKey, sizeof(callKey), "call_%s_%d_%d_%d_%d_0",
             callsign.c_str(), themes.textDim.r, themes.textDim.g,
             themes.textDim.b, callfontSize_);
    auto &callCache = getCachedText(renderer, callKey, callsign, themes.textDim,
                                    callfontSize_, false);
    if (callCache.texture) {
      SDL_Rect callDst = {x_ + pad, rowY, callCache.w, callCache.h};
      SDL_RenderCopy(renderer, callCache.texture, nullptr, &callDst);
    }

    for (const auto &a : active_) {
      if (a.index == i) {
        int cellX = x_ + pad + callWidth + a.bandIndex * bandWidth;
        SDL_Rect cell = {cellX + 2, rowY, bandWidth - 4, rowHeight - 1};
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &cell);
      }
    }
  }

  // Progress bar
  int barH = 2;
  SDL_Rect progressRect = {x_ + pad, y_ + height_ - barH - 2,
                           (int)((width_ - 2 * pad) * progress_), barH};
  SDL_SetRenderDrawColor(renderer, 0, 200, 255, 255);
  SDL_RenderFillRect(renderer, &progressRect);
}

void BeaconPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);

  // Clear texture cache if dimensions changed (font sizes will change)
  if (w != lastWidth_ || h != lastHeight_) {
    clearTextCache();
    lastWidth_ = w;
    lastHeight_ = h;
  }

  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  callfontSize_ = cat->ptSize(FontStyle::MediumBold);

  if (w < 100) {
    labelFontSize_ = cat->ptSize(FontStyle::Micro); // "NCDXF"
    callfontSize_ =
        cat->ptSize(FontStyle::Micro); // Frequencies (12px fits ~24px row)
  } else if (h < 120) {
    labelFontSize_ = cat->ptSize(FontStyle::Micro);
    callfontSize_ = cat->ptSize(FontStyle::Micro);
  }
}
