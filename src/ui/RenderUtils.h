#pragma once

#include <SDL2/SDL.h>

namespace RenderUtils {

// Draw a smooth line with a specific thickness using SDL_RenderGeometry.
void drawThickLine(SDL_Renderer *renderer, float x1, float y1, float x2,
                   float y2, float thickness, SDL_Color color);

// Draw a smooth filled circle using SDL_RenderGeometry.
void drawCircle(SDL_Renderer *renderer, float x, float y, float radius,
                SDL_Color color);

// Draw a filled rectangle using SDL_RenderGeometry.
void drawRect(SDL_Renderer *renderer, float x, float y, float w, float h,
              SDL_Color color);

// Draw a rectangle outline using SDL_RenderDrawRect.
void drawRectOutline(SDL_Renderer *renderer, float x, float y, float w, float h,
                     SDL_Color color);

// Draw a circle outline.
void drawCircleOutline(SDL_Renderer *renderer, float x, float y, float radius,
                       SDL_Color color);

// Draw a continuous thick line path using SDL_RenderGeometry (Triangle Strip).
void drawPolyline(SDL_Renderer *renderer, const SDL_FPoint *points, int count,
                  float thickness, SDL_Color color, bool closed = false);

} // namespace RenderUtils
