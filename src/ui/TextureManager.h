#pragma once

#include "../core/Logger.h"
#include "../core/MemoryMonitor.h"
#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <list>
#include <string>
#include <unordered_map>

class TextureManager {
public:
  TextureManager() = default;
  ~TextureManager() { clearCache(); }

  void clearCache() {
    for (auto &[key, entry] : cache_) {
      if (entry.texture) {
        destroyTexture(entry.texture);
      }
    }
    cache_.clear();
    lru_.clear();
  }

  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;

  SDL_Texture *loadBMP(SDL_Renderer *renderer, const std::string &key,
                       const std::string &path) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      touch(key);
      return it->second.texture;
    }
    pruneIfNecessary();
    SDL_Surface *surface = SDL_LoadBMP(path.c_str());
    if (!surface)
      return nullptr;
    SDL_Texture *texture = createTexture(renderer, surface, key);
    SDL_FreeSurface(surface);
    if (texture)
      insert(key, texture);
    return texture;
  }

  SDL_Texture *loadImage(SDL_Renderer *renderer, const std::string &key,
                         const std::string &path) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      touch(key);
      return it->second.texture;
    }
    pruneIfNecessary();
    SDL_Surface *surface = IMG_Load(path.c_str());
    if (!surface)
      return nullptr;
    SDL_Texture *texture = createTexture(renderer, surface, key);
    SDL_FreeSurface(surface);
    if (texture)
      insert(key, texture);
    return texture;
  }

  // Convenience overload for std::string payloads.
  SDL_Texture *loadFromMemory(SDL_Renderer *renderer, const std::string &key,
                              const std::string &data,
                              SDL_Color tint = {255, 255, 255, 255}) {
    return loadFromMemory(renderer, key,
                          reinterpret_cast<const unsigned char *>(data.data()),
                          static_cast<unsigned int>(data.size()), tint);
  }

  // Decode image bytes and upload to GPU, replacing any existing texture for
  // this key. Always re-decodes and re-uploads regardless of whether the data
  // has changed — callers must gate invocations on actual data changes (e.g.,
  // a dirty flag set by the background fetch callback) to avoid unnecessary
  // CPU decode and VRAM bandwidth usage.
  SDL_Texture *loadFromMemory(SDL_Renderer *renderer, const std::string &key,
                              const unsigned char *data, unsigned int size,
                              SDL_Color tint = {255, 255, 255, 255}) {
    // Determine max dimensions if not already known
    updateMaxDimensions(renderer);

    SDL_Surface *surface = decodeToSurface(data, size, key, tint, maxW_, maxH_);
    if (!surface)
      return nullptr;
    SDL_Texture *texture = loadFromSurface(renderer, key, surface);
    SDL_FreeSurface(surface);
    return texture;
  }

  // Upload an existing surface to GPU, replacing any existing texture for this
  // key. Note: The caller remains responsible for freeing the surface if
  // desired.
  SDL_Texture *loadFromSurface(SDL_Renderer *renderer, const std::string &key,
                               SDL_Surface *surface) {
    if (!surface)
      return nullptr;

    auto it = cache_.find(key);
    if (it != cache_.end()) {
      destroyTexture(it->second.texture);
      lru_.erase(it->second.lruIt);
      cache_.erase(it);
    } else {
      pruneIfNecessary();
    }

    SDL_Texture *texture = createTexture(renderer, surface, key);
    if (!texture && lowMemCallback_) {
      lowMemCallback_();
      texture = createTexture(renderer, surface, key);
    }

    if (texture) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      insert(key, texture);
    }
    return texture;
  }

  // Thread-safe CPU-bound image decoding and pixel manipulation.
  // Returns a new SDL_Surface that must be freed by the caller via
  // SDL_FreeSurface.
  static SDL_Surface *decodeToSurface(const unsigned char *data,
                                      unsigned int size,
                                      const std::string &key = "",
                                      SDL_Color tint = {255, 255, 255, 255},
                                      int maxW = 0, int maxH = 0) {
    SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw)
      return nullptr;

    SDL_Surface *surface = IMG_Load_RW(rw, 0);
    if (!surface) {
      SDL_RWseek(rw, 0, RW_SEEK_SET);
      surface = IMG_LoadTyped_RW(rw, 0, "PNG");
    }
    if (!surface) {
      SDL_RWseek(rw, 0, RW_SEEK_SET);
      surface = IMG_LoadTyped_RW(rw, 0, "JPG");
    }
    SDL_RWclose(rw);
    if (!surface)
      return nullptr;

    SDL_Surface *rgbaSurface =
        SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!rgbaSurface)
      return nullptr;
    surface = rgbaSurface;

    bool hasTint = (tint.r != 255 || tint.g != 255 || tint.b != 255);

    if (key == "nasa_moon" || key == "sdo_latest") {
      uint8_t *pixels = (uint8_t *)surface->pixels;
      for (int y = 0; y < surface->h; ++y) {
        uint32_t *row = (uint32_t *)(pixels + y * surface->pitch);
        for (int x = 0; x < surface->w; ++x) {
          uint32_t p = row[x];
          uint8_t r, g, b, a;
          SDL_GetRGBA(p, surface->format, &r, &g, &b, &a);
          uint8_t br = std::max({r, g, b});

          if (key == "nasa_moon") {
            if (br < 20)
              br = 0;
            else if (br < 100)
              br = (uint8_t)(((br - 20) / 80.0f) * br);
            row[x] = SDL_MapRGBA(surface->format, r, g, b, br);
          } else if (key == "sdo_latest") {
            if (hasTint) {
              // Map grayscale brightness to tint color
              r = (uint8_t)((br / 255.0f) * tint.r);
              g = (uint8_t)((br / 255.0f) * tint.g);
              b = (uint8_t)((br / 255.0f) * tint.b);
            }
            // Remove black background by setting alpha based on brightness.
            // We use a small threshold to kill compression noise.
            uint8_t alpha = 255;
            if (br < 15)
              alpha = 0;
            else if (br < 40)
              alpha = (uint8_t)(((br - 15) / 25.0f) * 255);

            row[x] = SDL_MapRGBA(surface->format, r, g, b, alpha);
          }
        }
      }
    }

    // Downscale if exceeds limits
    if (maxW > 0 && (surface->w > maxW || surface->h > maxH)) {
      float scale = std::min((float)maxW / surface->w, (float)maxH / surface->h);
      SDL_Surface *finalSurface = SDL_CreateRGBSurfaceWithFormat(
          0, (int)(surface->w * scale), (int)(surface->h * scale), 32,
          surface->format->format);
      if (finalSurface) {
        SDL_BlitScaled(surface, nullptr, finalSurface, nullptr);
        SDL_FreeSurface(surface);
        surface = finalSurface;
      }
    }

    return surface;
  }

  SDL_Texture *generateEarthFallback(SDL_Renderer *renderer,
                                     const std::string &key, int width,
                                     int height) {
    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                          SDL_TEXTUREACCESS_TARGET, width, height);
    if (!texture)
      return nullptr;
    SDL_SetRenderTarget(renderer, texture);
    SDL_SetRenderDrawColor(renderer, 10, 20, 60, 255);
    SDL_RenderClear(renderer);

    // Basic map outline
    SDL_SetRenderDrawColor(renderer, 50, 100, 200, 255);
    SDL_RenderDrawLine(renderer, 0, height / 2, width, height / 2);
    SDL_RenderDrawLine(renderer, width / 2, 0, width / 2, height);

    SDL_SetRenderTarget(renderer, nullptr);
    insert(key, texture);
    return texture;
  }

  SDL_Texture *generateLineTexture(SDL_Renderer *renderer,
                                   const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      touch(key);
      return it->second.texture;
    }
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormat(0, 1, 64, 32, SDL_PIXELFORMAT_RGBA32);
    uint32_t *p = (uint32_t *)s->pixels;
    for (int i = 0; i < 64; ++i) {
      float y = (i / 63.0f) * 2.0f - 1.0f;
      p[i] = SDL_MapRGBA(s->format, 255, 255, 255,
                         (uint8_t)(std::exp(-y * y * 8.0f) * 255));
    }
    SDL_Texture *t = createTexture(renderer, s, key);
    SDL_FreeSurface(s);
    if (t) {
      SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
      insert(key, t);
    }
    return t;
  }

  void generateMarkerTextures(SDL_Renderer *renderer) {
    if (cache_.count("marker_circle") && cache_.count("marker_square"))
      return;

    constexpr int sz = 64;
    SDL_Surface *cS =
        SDL_CreateRGBSurfaceWithFormat(0, sz, sz, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface *sS =
        SDL_CreateRGBSurfaceWithFormat(0, sz, sz, 32, SDL_PIXELFORMAT_RGBA32);
    if (!cS || !sS) {
      if (cS)
        SDL_FreeSurface(cS);
      if (sS)
        SDL_FreeSurface(sS);
      return;
    }

    uint32_t *cP = (uint32_t *)cS->pixels;
    uint32_t *sP = (uint32_t *)sS->pixels;
    float center = sz / 2.0f - 0.5f;
    float r = sz / 2.0f - 2.0f;

    for (int y = 0; y < sz; ++y) {
      for (int x = 0; x < sz; ++x) {
        float dx = x - center, dy = y - center;
        float dist = std::sqrt(dx * dx + dy * dy);
        uint8_t cA =
            (dist < r - 1.0f)
                ? 255
                : (dist < r + 1.0f
                       ? (uint8_t)((1.0f - (dist - (r - 1.0f)) / 2.0f) * 255)
                       : 0);
        float d = std::max(std::abs(dx), std::abs(dy));
        uint8_t sA =
            (d < r - 1.0f)
                ? 255
                : (d < r + 1.0f
                       ? (uint8_t)((1.0f - (d - (r - 1.0f)) / 2.0f) * 255)
                       : 0);
        cP[y * sz + x] = SDL_MapRGBA(cS->format, 255, 255, 255, cA);
        sP[y * sz + x] = SDL_MapRGBA(sS->format, 255, 255, 255, sA);
      }
    }

    SDL_Texture *ct = createTexture(renderer, cS, "marker_circle");
    SDL_Texture *st = createTexture(renderer, sS, "marker_square");
    SDL_FreeSurface(cS);
    SDL_FreeSurface(sS);
    if (ct) {
      SDL_SetTextureBlendMode(ct, SDL_BLENDMODE_BLEND);
      insert("marker_circle", ct);
    }
    if (st) {
      SDL_SetTextureBlendMode(st, SDL_BLENDMODE_BLEND);
      insert("marker_square", st);
    }
  }

  void generateWhiteTexture(SDL_Renderer *renderer) {
    if (cache_.count("white"))
      return;
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    *(uint32_t *)s->pixels = SDL_MapRGBA(s->format, 255, 255, 255, 255);
    SDL_Texture *t = createTexture(renderer, s, "white");
    SDL_FreeSurface(s);
    if (t) {
      SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
      insert("white", t);
    }
  }

  void generateBlackTexture(SDL_Renderer *renderer) {
    if (cache_.count("black"))
      return;
    SDL_Surface *s =
        SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    *(uint32_t *)s->pixels = SDL_MapRGBA(s->format, 0, 0, 0, 255);
    SDL_Texture *t = createTexture(renderer, s, "black");
    SDL_FreeSurface(s);
    if (t) {
      SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
      insert("black", t);
    }
  }

  SDL_Texture *get(const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      touch(key);
      return it->second.texture;
    }
    return nullptr;
  }

  void setLowMemCallback(std::function<void()> cb) { lowMemCallback_ = cb; }
  void setMaxCacheSize(int size) {
    maxCacheSize_ = size;
    pruneIfNecessary();
  }

  void updateMaxDimensions(SDL_Renderer *renderer) {
    if (maxW_ == 0) {
      SDL_RendererInfo info;
      if (SDL_GetRendererInfo(renderer, &info) == 0) {
        maxW_ = info.max_texture_width;
        maxH_ = info.max_texture_height;
#ifdef __EMSCRIPTEN__
        maxW_ = std::min(maxW_, 1024);
        maxH_ = std::min(maxH_, 1024);
#elif (defined(__arm__) || defined(__aarch64__)) && defined(__linux__)
        // RPi/Linux ARM only — keeps worst-case texture at 4 MB vs 16 MB.
        maxW_ = std::min(maxW_, 1024);
        maxH_ = std::min(maxH_, 1024);
#endif
      } else {
        maxW_ = 2048;
        maxH_ = 2048;
      }
    }
  }

  int getMaxW() const { return maxW_; }
  int getMaxH() const { return maxH_; }

private:
  struct CacheEntry {
    SDL_Texture *texture;
    std::list<std::string>::iterator lruIt;
  };
  void touch(const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      lru_.erase(it->second.lruIt);
      lru_.push_front(key);
      it->second.lruIt = lru_.begin();
    }
  }
  void insert(const std::string &key, SDL_Texture *texture) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      destroyTexture(it->second.texture);
      lru_.erase(it->second.lruIt);
    }
    lru_.push_front(key);
    cache_[key] = {texture, lru_.begin()};
  }
  void pruneIfNecessary() {
    while (cache_.size() >= (size_t)maxCacheSize_) {
      std::string oldest = lru_.back();
      destroyTexture(cache_[oldest].texture);
      cache_.erase(oldest);
      lru_.pop_back();
    }
  }
  SDL_Texture *createTexture(SDL_Renderer *renderer, SDL_Surface *surface,
                             const std::string &key) {
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, surface);
    if (t)
      MemoryMonitor::getInstance().addVram((int64_t)surface->w * surface->h *
                                           4);
    else
      LOG_E("TextureManager", "CreateTexture failed for {}", key);
    return t;
  }
  void destroyTexture(SDL_Texture *t) {
    int w, h;
    if (SDL_QueryTexture(t, nullptr, nullptr, &w, &h) == 0)
      MemoryMonitor::getInstance().markVramDestroyed((int64_t)w * h * 4);
    SDL_DestroyTexture(t);
  }
  std::unordered_map<std::string, CacheEntry> cache_;
  std::list<std::string> lru_;
  int maxW_ = 0, maxH_ = 0;
  int maxCacheSize_ = 50;
  std::function<void()> lowMemCallback_;
};
