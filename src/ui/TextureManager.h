#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <string>

class TextureManager {
public:
  TextureManager() = default;
  ~TextureManager() {
    for (auto &[key, tex] : cache_)
      SDL_DestroyTexture(tex);
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
      std::fprintf(stderr, "TextureManager: failed to load %s: %s\n",
                   path.c_str(), SDL_GetError());
      return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
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
      std::fprintf(stderr, "TextureManager: failed to load %s: %s\n",
                   path.c_str(), IMG_GetError());
      return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
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
    // If it already exists, destroy old one before replacing
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      SDL_DestroyTexture(it->second);
      cache_.erase(it);
    }

    SDL_RWops *rw = SDL_RWFromConstMem(data, static_cast<int>(size));
    if (!rw) {
      std::fprintf(stderr, "TextureManager: SDL_RWFromConstMem failed\n");
      return nullptr;
    }

    // std::fprintf(stderr, "TextureManager: decoding %s (%u bytes)...\n",
    //              key.c_str(), size);
    SDL_Surface *surface = IMG_Load_RW(rw, 1); // 1 = auto-close rw
    if (!surface) {
      std::fprintf(stderr, "TextureManager: IMG_Load_RW failed for %s: %s\n",
                   key.c_str(), IMG_GetError());
      return nullptr;
    }

    // std::fprintf(stderr, "TextureManager: decoded %s surface: %dx%d\n",
    //              key.c_str(), surface->w, surface->h);

    // Hardware Limit Check: Downscale if surface exceeds GPU's max texture size
    SDL_RendererInfo info;
    SDL_Surface *finalSurface = surface;
    bool mustFreeFinal = false;

    if (SDL_GetRendererInfo(renderer, &info) == 0) {
      if (info.max_texture_width > 0 && info.max_texture_height > 0 &&
          (surface->w > info.max_texture_width ||
           surface->h > info.max_texture_height)) {
        float scaleW = (float)info.max_texture_width / (float)surface->w;
        float scaleH = (float)info.max_texture_height / (float)surface->h;
        float scale = std::min(scaleW, scaleH);

        int newW = (int)(surface->w * scale);
        int newH = (int)(surface->h * scale);

        std::fprintf(stderr,
                     "TextureManager: image exceeds max texture size (%dx%d). "
                     "Downscaling to %dx%d...\n",
                     info.max_texture_width, info.max_texture_height, newW,
                     newH);

        finalSurface = SDL_CreateRGBSurfaceWithFormat(0, newW, newH, 32,
                                                      surface->format->format);
        if (finalSurface) {
          SDL_BlitScaled(surface, nullptr, finalSurface, nullptr);
          mustFreeFinal = true;
        } else {
          std::fprintf(stderr, "TextureManager: downscaling failed!\n");
          finalSurface = surface;
        }
      }
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, finalSurface);
    if (mustFreeFinal)
      SDL_FreeSurface(finalSurface);
    SDL_FreeSurface(surface);

    if (!texture) {
      std::fprintf(
          stderr,
          "TextureManager: SDL_CreateTextureFromSurface failed for %s: %s\n",
          key.c_str(), SDL_GetError());
      return nullptr;
    }

    std::fprintf(stderr,
                 "TextureManager: successfully created texture for %s\n",
                 key.c_str());
    cache_[key] = texture;
    return texture;
  }

  // Generate a procedural equirectangular Earth fallback (dark blue + grid
  // lines).
  SDL_Texture *generateEarthFallback(SDL_Renderer *renderer,
                                     const std::string &key, int width,
                                     int height) {
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    SDL_Texture *texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                          SDL_TEXTUREACCESS_TARGET, width, height);
    if (!texture)
      return nullptr;

    SDL_SetRenderTarget(renderer, texture);

    // Dark blue ocean
    SDL_SetRenderDrawColor(renderer, 10, 20, 60, 255);
    SDL_RenderClear(renderer);

    // Grid lines every 30 degrees
    SDL_SetRenderDrawColor(renderer, 40, 60, 100, 255);
    for (int lonDeg = -180; lonDeg <= 180; lonDeg += 30) {
      int px = static_cast<int>((lonDeg + 180.0) / 360.0 * width);
      SDL_RenderDrawLine(renderer, px, 0, px, height);
    }
    for (int latDeg = -90; latDeg <= 90; latDeg += 30) {
      int py = static_cast<int>((90.0 - latDeg) / 180.0 * height);
      SDL_RenderDrawLine(renderer, 0, py, width, py);
    }

    // Equator in slightly brighter color
    SDL_SetRenderDrawColor(renderer, 60, 90, 140, 255);
    int eqY = height / 2;
    SDL_RenderDrawLine(renderer, 0, eqY, width, eqY);

    // Prime meridian
    int pmX = width / 2;
    SDL_RenderDrawLine(renderer, pmX, 0, pmX, height);

    SDL_SetRenderTarget(renderer, nullptr);
    cache_[key] = texture;
    return texture;
  }

  // Generate a 1x64 texture with a central alpha peak for anti-aliased lines.
  SDL_Texture *generateLineTexture(SDL_Renderer *renderer,
                                   const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end())
      return it->second;

    constexpr int h = 64;
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_STATIC, 1, h);
    if (!texture)
      return nullptr;

    uint32_t pixels[h];
    for (int i = 0; i < h; ++i) {
      float y = (static_cast<float>(i) / (h - 1)) * 2.0f - 1.0f; // -1 to 1
      float alpha = std::exp(-y * y * 8.0f); // Gaussian falloff
      if (alpha < 1e-3f)
        alpha = 0;
      uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
      // RGBA8888: [R, G, B, A] in memory? SDL_BYTEORDER check would be better
      // but usually 0xRRGGBBAA or 0xAABBGGRR.
      // Let's use SDL_MapRGBA to be safe if we had a surface, but here we
      // just want white.
      pixels[i] = (0xFF << 24) | (0xFF << 16) | (0xFF << 8) | a;
    }

    SDL_UpdateTexture(texture, nullptr, pixels, 1 * sizeof(uint32_t));
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    cache_[key] = texture;
    return texture;
  }

  // Generate textures for markers (circle and square) with anti-aliased edges.
  void generateMarkerTextures(SDL_Renderer *renderer) {
    if (cache_.count("marker_circle") && cache_.count("marker_square"))
      return;

    constexpr int sz = 64;
    constexpr int center = sz / 2;
    constexpr float r = sz / 2.0f - 2.0f;

    // 1. Circle Marker
    SDL_Texture *circle = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STATIC, sz, sz);
    uint32_t *cPixels = new uint32_t[sz * sz];
    for (int y = 0; y < sz; ++y) {
      for (int x = 0; x < sz; ++x) {
        float dx = x - center + 0.5f;
        float dy = y - center + 0.5f;
        float dist = std::sqrt(dx * dx + dy * dy);
        float alpha = 0.0f;
        if (dist < r - 1.0f)
          alpha = 1.0f;
        else if (dist < r + 1.0f)
          alpha = 1.0f - (dist - (r - 1.0f)) / 2.0f;

        uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
        cPixels[y * sz + x] = (0xFF << 24) | (0xFF << 16) | (0xFF << 8) | a;
      }
    }
    SDL_UpdateTexture(circle, nullptr, cPixels, sz * sizeof(uint32_t));
    SDL_SetTextureBlendMode(circle, SDL_BLENDMODE_BLEND);
    cache_["marker_circle"] = circle;
    delete[] cPixels;

    // 2. Square Marker
    SDL_Texture *square = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STATIC, sz, sz);
    uint32_t *sPixels = new uint32_t[sz * sz];
    for (int y = 0; y < sz; ++y) {
      for (int x = 0; x < sz; ++x) {
        float dx = std::abs(x - center + 0.5f);
        float dy = std::abs(y - center + 0.5f);
        float d = std::max(dx, dy);
        float alpha = 0.0f;
        if (d < r - 1.0f)
          alpha = 1.0f;
        else if (d < r + 1.0f)
          alpha = 1.0f - (d - (r - 1.0f)) / 2.0f;

        uint8_t a = static_cast<uint8_t>(alpha * 255.0f);
        sPixels[y * sz + x] = (0xFF << 24) | (0xFF << 16) | (0xFF << 8) | a;
      }
    }
    SDL_UpdateTexture(square, nullptr, sPixels, sz * sizeof(uint32_t));
    SDL_SetTextureBlendMode(square, SDL_BLENDMODE_BLEND);
    cache_["marker_square"] = square;
    delete[] sPixels;
  }

  SDL_Texture *get(const std::string &key) const {
    auto it = cache_.find(key);
    return it != cache_.end() ? it->second : nullptr;
  }

private:
  std::map<std::string, SDL_Texture *> cache_;
};
