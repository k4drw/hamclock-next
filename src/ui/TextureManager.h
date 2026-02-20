#pragma once

#include "../core/Logger.h"
#include "../core/MemoryMonitor.h"
#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

class TextureManager {
public:
  TextureManager() = default;
  ~TextureManager() {
    for (auto &[key, tex] : cache_)
      destroyTexture(tex);
  }

  TextureManager(const TextureManager &) = delete;
  TextureManager &operator=(const TextureManager &) = delete;

  // Load a BMP texture from disk, cache by key. Returns nullptr on failure.
  SDL_Texture *loadBMP(SDL_Renderer *renderer, const std::string &key,
                       const std::string &path) {
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    SDL_Surface *surface = SDL_LoadBMP(path.c_str());
    if (!surface) {
      LOG_E("TextureManager", "Failed to load {}: {}", path, SDL_GetError());
      return nullptr;
    }
    SDL_Texture *texture = createTexture(renderer, surface, key);
    SDL_FreeSurface(surface);
    if (texture)
      cache_[key] = texture;
    return texture;
  }

  // Load any image (PNG, JPG, BMP, etc.) via SDL_image, cache by key.
  SDL_Texture *loadImage(SDL_Renderer *renderer, const std::string &key,
                         const std::string &path) {
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    SDL_Surface *surface = IMG_Load(path.c_str());
    if (!surface) {
      LOG_E("TextureManager", "Failed to load {}: {}", path, IMG_GetError());
      return nullptr;
    }
    SDL_Texture *texture = createTexture(renderer, surface, key);
    SDL_FreeSurface(surface);
    if (texture)
      cache_[key] = texture;
    return texture;
  }

  // Load an image from memory (e.g. bytes from NetworkManager).
  SDL_Texture *loadFromMemory(SDL_Renderer *renderer, const std::string &key,
                              const std::string &data) {
    return loadFromMemory(renderer, key,
                          reinterpret_cast<const unsigned char *>(data.data()),
                          static_cast<unsigned int>(data.size()));
  }

  // Load an image from memory (e.g. embedded assets).
  SDL_Texture *loadFromMemory(SDL_Renderer *renderer, const std::string &key,
                              const unsigned char *data, unsigned int size) {
    SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw) {
      LOG_E("TextureManager", "SDL_RWFromConstMem failed");
      return nullptr;
    }

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

    if (!surface) {
      LOG_E("TextureManager", "IMG_Load failed for {}: {}", key,
            IMG_GetError());
      return nullptr;
    }

    // Always convert to a consistent 32-bit format (RGBA32) to ensure
    // AlphaMod and BlendMode support across all drivers.
    SDL_Surface *rgbaSurface =
        SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!rgbaSurface) {
      LOG_E("TextureManager", "SDL_ConvertSurfaceFormat failed for {}", key);
      return nullptr;
    }
    surface = rgbaSurface;

    // Specialized Logic: Generate alpha channel from pixel brightness for
    // certain textures.
    if (key == "nasa_moon" || key == "sdo_latest") {
      uint8_t *pixels = (uint8_t *)surface->pixels;
      for (int y = 0; y < surface->h; ++y) {
        uint32_t *row = (uint32_t *)(pixels + y * surface->pitch);
        for (int x = 0; x < surface->w; ++x) {
          uint32_t p = row[x];
          uint8_t r, g, b, a;
          SDL_GetRGBA(p, surface->format, &r, &g, &b, &a);

          // Calculate brightness
          uint8_t br = std::max({r, g, b});

          // For the moon, we want to be more aggressive with black to avoid
          // JPEG artifacts around the edges showing up on non-black
          // backgrounds.
          if (key == "nasa_moon") {
            if (br < 20)
              br = 0;
            else if (br < 100) {
              // Smooth transition but faster than linear
              float f = (br - 20) / 80.0f;
              br = static_cast<uint8_t>(f * br);
            }
          }

          row[x] = SDL_MapRGBA(surface->format, r, g, b, br);
        }
      }
      LOG_I("TextureManager", "Generated alpha channel from brightness for {}",
            key);
    }

    // Hardware Limit Check: Downscale if surface exceeds GPU's max texture size
    if (maxW_ == 0 || maxH_ == 0) {
      SDL_RendererInfo info;
      if (SDL_GetRendererInfo(renderer, &info) == 0) {
        maxW_ = info.max_texture_width;
        maxH_ = info.max_texture_height;
        LOG_I("TextureManager", "GPU Max Texture Size: {}x{}", maxW_, maxH_);
#if defined(__linux__) || defined(__arm__) || defined(__aarch64__) ||          \
    defined(__EMSCRIPTEN__)
        // On RPi and WASM, GPU memory is limited.
        // Cap at 2048 to save memory. 5400x2700 RGBA32 is ~58MB!
        int cap = 2048;
#ifdef __EMSCRIPTEN__
        if (key == "earth_map" || key == "night_map") {
          cap = 1024;
        }
#endif
        if (maxW_ == 0 || maxW_ > cap) {
          maxW_ = cap;
          LOG_I("TextureManager", "Capping texture limit to {} for stability",
                cap);
        }
        if (maxH_ == 0 || maxH_ > cap) {
          maxH_ = cap;
        }
#endif
      } else {
#if defined(__linux__) || defined(__arm__) || defined(__aarch64__) ||          \
    defined(__EMSCRIPTEN__)
        maxW_ = 2048;
        maxH_ = 2048;
#else
        maxW_ = 8192;
        maxH_ = 8192;
#endif
      }
    }

    SDL_Surface *finalSurface = surface;
    bool mustFreeFinal = false;

    if (maxW_ > 0 && maxH_ > 0 && (surface->w > maxW_ || surface->h > maxH_)) {
      float scale =
          std::min((float)maxW_ / surface->w, (float)maxH_ / surface->h);
      int newW = (int)(surface->w * scale);
      int newH = (int)(surface->h * scale);
      LOG_W("TextureManager", "Downscaling {} to {}x{} (limit {}x{})", key,
            newW, newH, maxW_, maxH_);
      finalSurface = SDL_CreateRGBSurfaceWithFormat(0, newW, newH, 32,
                                                    surface->format->format);
      if (finalSurface) {
        if (SDL_BlitScaled(surface, nullptr, finalSurface, nullptr) != 0) {
          LOG_E("TextureManager", "SDL_BlitScaled failed: {}", SDL_GetError());
          SDL_FreeSurface(finalSurface);
          finalSurface = surface;
        } else {
          mustFreeFinal = true;
        }
      } else {
        LOG_E("TextureManager",
              "Failed to create surface for {} downscale: {}. "
              "Source is {}x{}, Target was {}x{}.",
              key, SDL_GetError(), surface->w, surface->h, newW, newH);

        // CRITICAL: If we are already low on RAM, the 58MB source surface plus
        // the 8MB dest surface might be too much.
        // We will try one more time with a TINY surface just to survive.
        finalSurface = SDL_CreateRGBSurfaceWithFormat(0, 512, 256, 32,
                                                      surface->format->format);
        if (finalSurface) {
          LOG_W("TextureManager", "Resort to 512x256 fallback for {}", key);
          SDL_BlitScaled(surface, nullptr, finalSurface, nullptr);
          mustFreeFinal = true;
        } else {
          finalSurface =
              surface; // Last resort, will likely fail SDL_CreateTexture
        }
      }
    }

    // Memory optimization: destroy previous texture before creating the new one
    // to avoid peak VRAM usage.
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      destroyTexture(it->second);
      cache_.erase(it);
    }

    SDL_Texture *texture = createTexture(renderer, finalSurface, key);
    if (!texture) {
      // If we failed, try to flush fonts and try once more
      LOG_W("TextureManager",
            "Allocation failed, trying to flush FontManager and retry...");
      if (lowMemCallback_)
        lowMemCallback_();
      texture = createTexture(renderer, finalSurface, key);
    }

    if (mustFreeFinal)
      SDL_FreeSurface(finalSurface);
    SDL_FreeSurface(surface);

    if (!texture) {
      return nullptr;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    cache_[key] = texture;
    LOG_I("TextureManager", "Created texture for {}", key);
    MemoryMonitor::getInstance().logStats("TextureManager post-load");
    return texture;
  }

  // Generate a procedural equirectangular Earth fallback.
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
    SDL_SetRenderDrawColor(renderer, 40, 60, 100, 255);
    for (int lonDeg = -180; lonDeg <= 180; lonDeg += 30) {
      int px = static_cast<int>((lonDeg + 180.0) / 360.0 * width);
      SDL_RenderDrawLine(renderer, px, 0, px, height);
    }
    for (int latDeg = -90; latDeg <= 90; latDeg += 30) {
      int py = static_cast<int>((90.0 - latDeg) / 180.0 * height);
      SDL_RenderDrawLine(renderer, 0, py, width, py);
    }
    SDL_SetRenderTarget(renderer, nullptr);

    // Track target texture
    MemoryMonitor::getInstance().addVram(width * height * 4);

    cache_[key] = texture;
    return texture;
  }

  // Generate a procedural 1x64 texture for anti-aliased lines.
  SDL_Texture *generateLineTexture(SDL_Renderer *renderer,
                                   const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    constexpr int h = 64;
    SDL_Surface *surf =
        SDL_CreateRGBSurfaceWithFormat(0, 1, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf)
      return nullptr;
    uint32_t *pix = (uint32_t *)surf->pixels;
    for (int i = 0; i < h; ++i) {
      float y = (static_cast<float>(i) / (h - 1)) * 2.0f - 1.0f;
      float alpha = std::exp(-y * y * 8.0f);
      if (alpha < 0.001f)
        alpha = 0;
      pix[i] =
          SDL_MapRGBA(surf->format, 255, 255, 255, (uint8_t)(alpha * 255.0f));
    }
    SDL_Texture *tex = createTexture(renderer, surf, key);
    SDL_FreeSurface(surf);
    if (tex) {
      SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
      cache_[key] = tex;
    }
    return tex;
  }

  // Generate circle and square markers.
  void generateMarkerTextures(SDL_Renderer *renderer) {
    if (cache_.count("marker_circle") && cache_.count("marker_square"))
      return;

    constexpr int sz = 64;
    constexpr int center = sz / 2;
    constexpr float r = sz / 2.0f - 2.0f;

    SDL_Surface *cSurf =
        SDL_CreateRGBSurfaceWithFormat(0, sz, sz, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Surface *sSurf =
        SDL_CreateRGBSurfaceWithFormat(0, sz, sz, 32, SDL_PIXELFORMAT_RGBA32);
    if (!cSurf || !sSurf) {
      if (cSurf)
        SDL_FreeSurface(cSurf);
      if (sSurf)
        SDL_FreeSurface(sSurf);
      return;
    }

    uint32_t *cPix = (uint32_t *)cSurf->pixels;
    uint32_t *sPix = (uint32_t *)sSurf->pixels;

    for (int y = 0; y < sz; ++y) {
      for (int x = 0; x < sz; ++x) {
        float dx = x - center + 0.5f;
        float dy = y - center + 0.5f;
        float dist = std::sqrt(dx * dx + dy * dy);
        float cA =
            (dist < r - 1.0f)
                ? 1.0f
                : (dist < r + 1.0f ? 1.0f - (dist - (r - 1.0f)) / 2.0f : 0.0f);
        float d = std::max(std::abs(dx), std::abs(dy));
        float sA = (d < r - 1.0f)
                       ? 1.0f
                       : (d < r + 1.0f ? 1.0f - (d - (r - 1.0f)) / 2.0f : 0.0f);

        cPix[y * sz + x] =
            SDL_MapRGBA(cSurf->format, 255, 255, 255, (uint8_t)(cA * 255));
        sPix[y * sz + x] =
            SDL_MapRGBA(sSurf->format, 255, 255, 255, (uint8_t)(sA * 255));
      }
    }

    SDL_Texture *ct = createTexture(renderer, cSurf, "marker_circle");
    SDL_Texture *st = createTexture(renderer, sSurf, "marker_square");
    if (ct) {
      SDL_SetTextureBlendMode(ct, SDL_BLENDMODE_BLEND);
      cache_["marker_circle"] = ct;
    }
    if (st) {
      SDL_SetTextureBlendMode(st, SDL_BLENDMODE_BLEND);
      cache_["marker_square"] = st;
    }
    SDL_FreeSurface(cSurf);
    SDL_FreeSurface(sSurf);
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
      cache_["white"] = t;
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
      cache_["black"] = t;
    }
  }

  SDL_Texture *get(const std::string &key) const {
    auto it = cache_.find(key);
    return it != cache_.end() ? it->second : nullptr;
  }

  void setLowMemCallback(std::function<void()> cb) { lowMemCallback_ = cb; }

private:
  SDL_Texture *createTexture(SDL_Renderer *renderer, SDL_Surface *surface,
                             const std::string &key) {
    if (!surface)
      return nullptr;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
      LOG_E("TextureManager", "SDL_CreateTextureFromSurface failed for {}: {}",
            key, SDL_GetError());
      return nullptr;
    }

    // Track estimated VRAM (assuming 4 bytes per pixel for typical textures)
    int64_t bytes = surface->w * surface->h * 4;
    MemoryMonitor::getInstance().addVram(bytes);

    return texture;
  }

  void destroyTexture(SDL_Texture *texture) {
    if (!texture)
      return;
    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    MemoryMonitor::getInstance().markVramDestroyed(static_cast<int64_t>(w) * h *
                                                   4);
    SDL_DestroyTexture(texture);
  }

  std::map<std::string, SDL_Texture *> cache_;
  int maxW_ = 0;
  int maxH_ = 0;
  std::function<void()> lowMemCallback_;
};
