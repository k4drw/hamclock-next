#pragma once

#include <SDL.h>

// SDL_RenderGeometry was added in 2.0.18.
// We provide a compatibility layer for older versions (like those on some RPi
// distros).
#if !SDL_VERSION_ATLEAST(2, 0, 18)

typedef struct SDL_Vertex {
  SDL_FPoint position;
  SDL_Color color;
  SDL_FPoint tex_coord;
} SDL_Vertex;

/**
 * Basic software fallback for SDL_RenderGeometry when using older SDL2.
 * Note: This is an incomplete polyfill for critical build compatibility.
 */
static inline int SDL_RenderGeometry(SDL_Renderer *renderer,
                                     SDL_Texture *texture,
                                     const SDL_Vertex *vertices,
                                     int num_vertices, const int *indices,
                                     int num_indices) {
  // For our specific uses in HamClock, we mostly use this for filled polygons.
  // A full triangle rasterizer is too much.
  // We'll return an error or handle specific cases via #if in calling code.
  return -1;
}

#endif
