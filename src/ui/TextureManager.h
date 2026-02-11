#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <cstdio>
#include <map>
#include <string>

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager() {
        for (auto& [key, tex] : cache_) SDL_DestroyTexture(tex);
    }

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    // Load a BMP texture from disk, cache by key. Returns nullptr on failure.
    SDL_Texture* loadBMP(SDL_Renderer* renderer, const std::string& key,
                         const std::string& path) {
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        SDL_Surface* surface = SDL_LoadBMP(path.c_str());
        if (!surface) {
            std::fprintf(stderr, "TextureManager: failed to load %s: %s\n",
                         path.c_str(), SDL_GetError());
            return nullptr;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture) cache_[key] = texture;
        return texture;
    }

    // Load any image (PNG, JPG, BMP, etc.) via SDL_image, cache by key.
    SDL_Texture* loadImage(SDL_Renderer* renderer, const std::string& key,
                           const std::string& path) {
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        SDL_Surface* surface = IMG_Load(path.c_str());
        if (!surface) {
            std::fprintf(stderr, "TextureManager: failed to load %s: %s\n",
                         path.c_str(), IMG_GetError());
            return nullptr;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (texture) cache_[key] = texture;
        return texture;
    }

    // Generate a procedural equirectangular Earth fallback (dark blue + grid lines).
    SDL_Texture* generateEarthFallback(SDL_Renderer* renderer, const std::string& key,
                                       int width, int height) {
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        SDL_Texture* texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
        if (!texture) return nullptr;

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

    SDL_Texture* get(const std::string& key) const {
        auto it = cache_.find(key);
        return it != cache_.end() ? it->second : nullptr;
    }

private:
    std::map<std::string, SDL_Texture*> cache_;
};
