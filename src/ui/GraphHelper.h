#pragma once

#include "RenderUtils.h"
#include <SDL.h>

// Shared helpers for panel graph rendering.
namespace GraphHelper {

// Draw a framed time-series graph from pre-built point array.
// Handles frame border, textured/plain polyline dispatch, and
// single-point dot fallback.
inline void drawTimeSeries(SDL_Renderer *renderer, SDL_Texture *lineTex,
                           int x, int y, int w, int h,
                           const SDL_FPoint *pts, int count,
                           SDL_Color lineColor, SDL_Color borderColor,
                           float thickness = 2.0f) {
  SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g,
                         borderColor.b, 150);
  SDL_Rect frame = {x, y, w, h};
  SDL_RenderDrawRect(renderer, &frame);

  if (count == 0) return;

  if (lineTex && count > 1) {
    RenderUtils::drawPolylineTextured(renderer, lineTex, pts, count, thickness,
                                      lineColor);
  } else if (count > 1) {
    RenderUtils::drawPolyline(renderer, pts, count, thickness, lineColor);
  } else {
    // Single-point: render a 2x2 dot for visibility.
    SDL_SetRenderDrawColor(renderer, lineColor.r, lineColor.g, lineColor.b,
                           lineColor.a);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x,     (int)pts[0].y);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x + 1, (int)pts[0].y);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x,     (int)pts[0].y + 1);
    SDL_RenderDrawPoint(renderer, (int)pts[0].x + 1, (int)pts[0].y + 1);
  }
}

} // namespace GraphHelper
