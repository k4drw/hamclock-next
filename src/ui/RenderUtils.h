#pragma once

#include <SDL.h>

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

// Draw a filled triangle.
void drawTriangle(SDL_Renderer *renderer, float x1, float y1, float x2,
                  float y2, float x3, float y3, SDL_Color color);

// Draw a triangle outline.
void drawTriangleOutline(SDL_Renderer *renderer, float x1, float y1, float x2,
                         float y2, float x3, float y3, SDL_Color color);

// Draw a continuous thick line path using SDL_RenderGeometry (Triangle Strip).
void drawPolyline(SDL_Renderer *renderer, const SDL_FPoint *points, int count,
                  float thickness, SDL_Color color, bool closed = false);

// Draw a textured thick line for anti-aliasing.
void drawThickLineTextured(SDL_Renderer *renderer, SDL_Texture *tex, float x1,
                           float y1, float x2, float y2, float thickness,
                           SDL_Color color);

// Draw a textured polyline for anti-aliasing.
void drawPolylineTextured(SDL_Renderer *renderer, SDL_Texture *tex,
                          const SDL_FPoint *points, int count, float thickness,
                          SDL_Color color, bool closed = false);

// Draw a procedural gear icon.
void drawGear(SDL_Renderer *renderer, float x, float y, float radius,
              SDL_Color color, SDL_Color centerColor);

} // namespace RenderUtils
