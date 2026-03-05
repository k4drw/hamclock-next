#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include "../core/MemoryMonitor.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

class FontCatalog; // forward declaration

class FontManager {
public:
  FontManager() = default;
  ~FontManager() {
    closeAll();
    clearCache();
  }

  void setCatalog(FontCatalog *cat) { catalog_ = cat; }
  FontCatalog *catalog() const { return catalog_; }

  // Render scale: physicalOutputHeight / logicalHeight (e.g., 1080/480 = 2.25).
  // When > 1.0, text is super-sampled at physical resolution for crispness.
  void setRenderScale(float scale) { renderScale_ = std::max(1.0f, scale); }
  float renderScale() const { return renderScale_; }

  // Tune the persistent text cache limit at runtime (default: 300).
  // Call with a lower value (e.g. 100) on low-memory devices.
  void setTextCacheLimit(size_t limit) { textCacheLimit_ = limit; }

  FontManager(const FontManager &) = delete;
  FontManager &operator=(const FontManager &) = delete;

  bool loadFromMemory(const unsigned char *data, unsigned int size,
                      int defaultPtSize) {
    closeAll();
    data_ = data;
    size_ = size;
    defaultSize_ = defaultPtSize;
    return getFont(defaultPtSize) != nullptr;
  }

  bool ready() const { return data_ != nullptr; }

  // Get a font at the requested point size (cached).
  TTF_Font *getFont(int ptSize) {
    ptSize = std::clamp(ptSize, 8, 600);
    auto it = cache_.find(ptSize);
    if (it != cache_.end())
      return it->second;

    if (!data_)
      return nullptr;

    SDL_RWops *rw = SDL_RWFromConstMem(data_, size_);
    if (!rw)
      return nullptr;

    TTF_Font *font = TTF_OpenFontRW(rw, 1, ptSize); // 1 = auto-close rw
    if (!font) {
      std::fprintf(stderr,
                   "FontManager: failed to open embedded font at %dpt: %s\n",
                   ptSize, TTF_GetError());
      return nullptr;
    }
    cache_[ptSize] = font;
    return font;
  }

  // Get font sized to approximately fill targetHeight pixels (roughly 60% of
  // height).
  TTF_Font *getScaledFont(int targetHeight) {
    int ptSize = std::max(8, static_cast<int>(targetHeight * 0.6f));
    return getFont(ptSize);
  }

  // Creates an SDL_Texture from text at a specific point size. Caller owns the
  // texture. When renderScale_ > 1.0, text is rasterized at physical
  // resolution. The texture will be physically larger than logical size.
  // Returns logical dimensions via outW, outH if provided.
  SDL_Texture *renderText(SDL_Renderer *renderer, const std::string &text,
                          SDL_Color color, int ptSize = 0, int *outW = nullptr,
                          int *outH = nullptr, bool bold = false) {
    if (text.empty())
      return nullptr;
    int basePt = ptSize > 0 ? ptSize : defaultSize_;

    // Calculate size to render at
    int renderPt = basePt;
    if (renderScale_ > 1.01f) {
      renderPt = std::clamp(static_cast<int>(basePt * renderScale_), 8, 600);
    }

    TTF_Font *font = getFont(renderPt);
    if (!font)
      return nullptr;

    // Apply bold style if requested
    int prevStyle = TTF_GetFontStyle(font);
    if (bold) {
      TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
    }

    // Detect if we actually need wrapping
    bool hasNewline = (text.find('\n') != std::string::npos);

    SDL_Surface *surface = nullptr;
    if (!hasNewline) {
      // Use non-wrapped version for single lines.
      // This ensures list boxes and normal UI text never wrap unexpectedly.
      surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    } else {
      // Multi-line text.
      // Calculate exact required width to avoid the 2048px greedy surface
      // issue. We add a small 4px buffer to account for rounding errors.
      int measuredW = getLogicalWidth(text, basePt, bold);
      int physicalWrapWidth =
          std::max(1, static_cast<int>(measuredW * renderScale_) + 4);
      surface = TTF_RenderUTF8_Blended_Wrapped(font, text.c_str(), color,
                                               physicalWrapWidth);
    }

    // Restore previous style
    if (bold) {
      TTF_SetFontStyle(font, prevStyle);
    }

    if (!surface)
      return nullptr;

    // Hardware Limit Check: Sanity check dimensions before trying to allocate
    // GPU memory. RPi KMSDRM has a 2048px effective reliable limit.
    if (maxW_ == 0 || maxH_ == 0) {
      SDL_RendererInfo info;
      if (SDL_GetRendererInfo(renderer, &info) == 0) {
        maxW_ = info.max_texture_width;
        maxH_ = info.max_texture_height;
#if (defined(__arm__) || defined(__aarch64__)) && defined(__linux__)
        // RPi/Linux ARM only — not macOS Apple Silicon.
        if (maxW_ > 1024)
          maxW_ = 1024;
        if (maxH_ > 1024)
          maxH_ = 1024;
#endif
      }
    }

    if (maxW_ > 0 && maxH_ > 0 && (surface->w > maxW_ || surface->h > maxH_)) {
      std::fprintf(stderr,
                   "FontManager: surface too large for GPU (%dx%d > %dx%d), "
                   "clipping. Text='%s'\n",
                   surface->w, surface->h, maxW_, maxH_,
                   text.substr(0, 40).c_str());
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
      // Log detailed error - this may indicate GPU memory exhaustion
      std::fprintf(stderr,
                   "FontManager: SDL_CreateTextureFromSurface failed "
                   "(text='%s', size=%dx%d): %s\n",
                   text.substr(0, 40).c_str(), surface->w, surface->h,
                   SDL_GetError());
      SDL_FreeSurface(surface);
      return nullptr;
    }

    // VRAM accounting (w, h are already physical pixels)
    int64_t bytes = static_cast<int64_t>(surface->w) * surface->h * 4;
    MemoryMonitor::getInstance().addVram(bytes);

    // Logical dimensions for caller
    if (outW)
      *outW = static_cast<int>(surface->w / renderScale_);
    if (outH)
      *outH = static_cast<int>(surface->h / renderScale_);

    // Enable linear filtering for smooth scaling
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);
    return texture;
  }

  // Convenience: render + draw at (x, y). Internal cache prevents per-frame
  // texture churn, dramatic optimization for embedded devices.
  void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y,
                SDL_Color color, int ptSize = 0, bool bold = false,
                bool centered = false, bool forceVolatile = false) {
    if (text.empty())
      return;

    int basePt = ptSize > 0 ? ptSize : defaultSize_;

    // Heuristic for volatile text (timers, etc.) that changes every frame
    bool volatileText =
        forceVolatile ||
        (text.length() >= 5 &&
         ((text.find(':') != std::string::npos &&
           text.find(':', text.find(':') + 1) != std::string::npos) ||
          text.find("Up ") != std::string::npos ||
          (text.find('s') != std::string::npos &&
           text.find('m') != std::string::npos)));

    if (volatileText) {
      VolatileCacheKey key{x, y, basePt, bold, color};
      auto it = volatileCache_.find(key);

      // If we have a cached texture and the text is unchanged, just draw it.
      if (it != volatileCache_.end() && it->second.text == text) {
        it->second.lastUsed = SDL_GetTicks();
        SDL_Rect dst = {x, y, it->second.w, it->second.h};
        if (centered) {
          dst.x -= it->second.w / 2;
          dst.y -= it->second.h / 2;
        }
        SDL_RenderCopy(renderer, it->second.texture, nullptr, &dst);
        return;
      }

      // Text has changed or is new, we need to re-render.
      int w = 0, h = 0;
      SDL_Texture *tex =
          renderText(renderer, text, color, basePt, &w, &h, bold);
      if (!tex)
        return;

      // If an old texture was here, destroy it.
      if (it != volatileCache_.end()) {
        MemoryMonitor::getInstance().destroyTexture(it->second.texture);
      }

      // Store the new texture and its text content in the volatile cache.
      volatileCache_[key] = {tex, w, h, SDL_GetTicks(), text};

      SDL_Rect dst = {x, y, w, h};
      if (centered) {
        dst.x -= w / 2;
        dst.y -= h / 2;
      }
      SDL_RenderCopy(renderer, tex, nullptr, &dst);

    } else {
      // Logic for non-volatile, persistent text
      TextCacheKey key{text, color, basePt, bold};
      auto it = textCache_.find(key);
      if (it != textCache_.end()) {
        it->second.lastUsed = SDL_GetTicks();
        SDL_Rect dst = {x, y, it->second.w, it->second.h};
        if (centered) {
          dst.x -= it->second.w / 2;
          dst.y -= it->second.h / 2;
        }
        SDL_RenderCopy(renderer, it->second.texture, nullptr, &dst);
        return;
      }

      // Not in cache - render new texture
      int w = 0, h = 0;
      SDL_Texture *tex =
          renderText(renderer, text, color, ptSize, &w, &h, bold);
      if (!tex)
        return;

      // Prune cache if it gets too large
      if (textCache_.size() > textCacheLimit_) {
        pruneCache();
      }
      // Add to cache
      textCache_[key] = {tex, w, h, SDL_GetTicks()};

      SDL_Rect dst = {x, y, w, h};
      if (centered) {
        dst.x -= w / 2;
        dst.y -= h / 2;
      }
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
    }
  }

  // Returns the width of the text in logical units.
  // Correctly accounts for renderScale_ and super-sampling.
  int getLogicalWidth(const std::string &text, int ptSize = 0,
                      bool bold = false) {
    if (text.empty())
      return 0;
    int basePt = ptSize > 0 ? ptSize : defaultSize_;
    int renderPt = basePt;
    if (renderScale_ > 1.01f) {
      renderPt = std::clamp(static_cast<int>(basePt * renderScale_), 8, 600);
    }
    TTF_Font *font = getFont(renderPt);
    if (!font)
      return 0;

    int prevStyle = TTF_GetFontStyle(font);
    if (bold) {
      TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
    }
    int maxW = 0;
    size_t start = 0;
    while (start < text.length()) {
      size_t end = text.find('\n', start);
      std::string line =
          text.substr(start, (end == std::string::npos) ? std::string::npos
                                                        : (end - start));
      int w = 0, h = 0;
      TTF_SizeUTF8(font, line.empty() ? " " : line.c_str(), &w, &h);
      if (w > maxW)
        maxW = w;
      if (end == std::string::npos)
        break;
      start = end + 1;
    }

    if (bold) {
      TTF_SetFontStyle(font, prevStyle);
    }

    // Add 1px buffer for safety against rounding issues
    return static_cast<int>((maxW + 1) / renderScale_);
  }

  // Returns the height of the text in logical units.
  // Correctly accounts for multi-line text and renderScale_.
  int getLogicalHeight(const std::string &text, int ptSize = 0,
                       bool bold = false, int wrapWidth = 2048) {
    if (text.empty())
      return 0;
    int basePt = ptSize > 0 ? ptSize : defaultSize_;
    int renderPt = basePt;
    if (renderScale_ > 1.01f) {
      renderPt = std::clamp(static_cast<int>(basePt * renderScale_), 8, 600);
    }
    TTF_Font *font = getFont(renderPt);
    if (!font)
      return 0;

    int prevStyle = TTF_GetFontStyle(font);
    if (bold) {
      TTF_SetFontStyle(font, prevStyle | TTF_STYLE_BOLD);
    }

    int lineCount = 0;
    int fontHeight = TTF_FontHeight(font);
    size_t start = 0;
    while (start < text.length()) {
      lineCount++;
      size_t end = text.find('\n', start);
      if (end == std::string::npos)
        break;
      start = end + 1;
    }
    // Handle trailing newline
    if (!text.empty() && text.back() == '\n')
      lineCount++;

    if (bold) {
      TTF_SetFontStyle(font, prevStyle);
    }

    return static_cast<int>((fontHeight * std::max(1, lineCount)) /
                            renderScale_);
  }

  void clearCache() {
    for (auto &[key, val] : textCache_) {
      MemoryMonitor::getInstance().destroyTexture(val.texture);
    }
    textCache_.clear();
    for (auto &[key, val] : volatileCache_) {
      MemoryMonitor::getInstance().destroyTexture(val.texture);
    }
    volatileCache_.clear();
  }

private:
  struct TextCacheKey {
    std::string text;
    SDL_Color color;
    int ptSize;
    bool bold;

    bool operator<(const TextCacheKey &other) const {
      if (text != other.text)
        return text < other.text;
      if (ptSize != other.ptSize)
        return ptSize < other.ptSize;
      if (bold != other.bold)
        return bold < other.bold;
      if (color.r != other.color.r)
        return color.r < other.color.r;
      if (color.g != other.color.g)
        return color.g < other.color.g;
      if (color.b != other.color.b)
        return color.b < other.color.b;
      return color.a < other.color.a;
    }
  };

  struct VolatileCacheKey {
    int x, y, ptSize;
    bool bold;
    SDL_Color color;

    bool operator<(const VolatileCacheKey &other) const {
      if (x != other.x)
        return x < other.x;
      if (y != other.y)
        return y < other.y;
      if (ptSize != other.ptSize)
        return ptSize < other.ptSize;
      if (bold != other.bold)
        return bold < other.bold;
      if (color.r != other.color.r)
        return color.r < other.color.r;
      if (color.g != other.color.g)
        return color.g < other.color.g;
      if (color.b != other.color.b)
        return color.b < other.color.b;
      return color.a < other.color.a;
    }
  };

  struct CachedTexture {
    SDL_Texture *texture;
    int w, h;
    uint32_t lastUsed;
  };

  struct CachedTextureWithText {
    SDL_Texture *texture;
    int w, h;
    uint32_t lastUsed;
    std::string text;
  };

  void pruneCache() {
    // Evict the oldest 50% of text cache entries to prevent massive VRAM churn
    if (textCache_.empty())
      return;

    std::vector<std::pair<TextCacheKey, CachedTexture>> entries(
        textCache_.begin(), textCache_.end());

    std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
      return a.second.lastUsed < b.second.lastUsed;
    });

    size_t evictCount = std::max((size_t)1, textCache_.size() / 2);
    for (size_t i = 0; i < evictCount; ++i) {
      MemoryMonitor::getInstance().destroyTexture(entries[i].second.texture);
      textCache_.erase(entries[i].first);
    }
  }

  void closeAll() {
    for (auto &[size, font] : cache_) {
      TTF_CloseFont(font);
    }
    cache_.clear();
  }

  const unsigned char *data_ = nullptr;
  unsigned int size_ = 0;
  int defaultSize_ = 24;
  float renderScale_ = 1.0f;
  size_t textCacheLimit_ = 300;
  std::map<int, TTF_Font *> cache_;
  std::map<TextCacheKey, CachedTexture> textCache_;
  std::map<VolatileCacheKey, CachedTextureWithText> volatileCache_;
  FontCatalog *catalog_ = nullptr;
  int maxW_ = 0;
  int maxH_ = 0;
};
