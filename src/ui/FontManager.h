#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

class FontCatalog; // forward declaration

class FontManager {
public:
  FontManager() = default;
  ~FontManager() { closeAll(); }

  void setCatalog(FontCatalog *cat) { catalog_ = cat; }
  FontCatalog *catalog() const { return catalog_; }

  // Render scale: physicalOutputHeight / logicalHeight (e.g., 1080/480 = 2.25).
  // When > 1.0, text is super-sampled at physical resolution for crispness.
  void setRenderScale(float scale) { renderScale_ = std::max(1.0f, scale); }
  float renderScale() const { return renderScale_; }

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

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);

    // Restore previous style
    if (bold) {
      TTF_SetFontStyle(font, prevStyle);
    }

    if (!surface)
      return nullptr;

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

    // Logical dimensions
    if (outW)
      *outW = static_cast<int>(surface->w / renderScale_);
    if (outH)
      *outH = static_cast<int>(surface->h / renderScale_);

    // Enable linear filtering for smooth scaling if needed
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);
    return texture;
  }

  // Convenience: render + draw at (x, y). Not cached — use for one-off draws
  // only.
  void drawText(SDL_Renderer *renderer, const std::string &text, int x, int y,
                SDL_Color color, int ptSize = 0, bool bold = false,
                bool centered = false) {
    int w = 0, h = 0;
    SDL_Texture *tex = renderText(renderer, text, color, ptSize, &w, &h, bold);
    if (!tex)
      return;
    SDL_Rect dst = {x, y, w, h};
    if (centered) {
      dst.x -= w / 2;
      dst.y -= h / 2;
    }
    SDL_RenderCopy(renderer, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
  }

private:
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
  std::map<int, TTF_Font *> cache_;
  FontCatalog *catalog_ = nullptr;
};
