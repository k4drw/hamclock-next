#include "BeaconPanel.h"
#include "../core/BeaconData.h"
#include "../core/Logger.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cstdio>

BeaconPanel::BeaconPanel(int x, int y, int w, int h, FontManager &fontMgr,
                         BeaconProvider &provider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), provider_(provider) {}

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

  renderChrome(renderer);

  bool isNarrow = (width_ < 100 || height_ < 140);
  int titleH = 20;

  // Standard Title
  const char *titleText = isNarrow ? "NCDXF" : "NCDXF Beacons";
  auto *cat = fontMgr_.catalog();
  FontStyle titleStyle = isNarrow ? FontStyle::CaptionBold : FontStyle::MicroBold;
  cat->drawText(renderer, titleText, x_ + 10, y_ + 5, themes.accent, titleStyle);

  if (isNarrow) {
    // Narrow Layout
    int pad = 4;
    int curY = y_ + titleH + pad;

    // (Old layout-specific title was here, now replaced by standard one above)

    // Band colors (standardized with theme tokens)
    SDL_Color bandColors[] = {
        themes.warning, // 20m: Yellow/Warning
        themes.success, // 17m: Green/Success
        themes.info,    // 15m: Cyan/Info
        themes.info,    // 12m: Info
        themes.accent,  // 10m: Accent
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
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
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
        SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, themes.success.a);
        SDL_RenderFillRect(renderer, &cell);
      }
    }
  }

  // Progress bar
  int barH = 2;
  SDL_Rect progressRect = {x_ + pad, y_ + height_ - barH - 2,
                           (int)((width_ - 2 * pad) * progress_), barH};
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, themes.accent.a);
  SDL_RenderFillRect(renderer, &progressRect);
}

void BeaconPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);

  // Clear texture cache if dimensions or render scale changed
  float currentScale = fontMgr_.renderScale();
  if (w != lastWidth_ || h != lastHeight_ || currentScale != lastRenderScale_) {
    clearTextCache();
    lastWidth_ = w;
    lastHeight_ = h;
    lastRenderScale_ = currentScale;
  }

  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  callfontSize_ = cat->ptSize(FontStyle::MediumBold);

  if (w < 100 || h < 140) {
    labelFontSize_ = cat->ptSize(FontStyle::Micro); // "NCDXF"
    callfontSize_ =
        cat->ptSize(FontStyle::FastBold); // Frequencies
  } else {
    labelFontSize_ = cat->ptSize(FontStyle::FastBold);
    callfontSize_ = cat->ptSize(FontStyle::Micro);
  }
}
