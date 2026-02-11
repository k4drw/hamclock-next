#include "RenderUtils.h"
#include <cmath>
#include <vector>

namespace RenderUtils {

void drawThickLine(SDL_Renderer *renderer, float x1, float y1, float x2,
                   float y2, float thickness, SDL_Color color) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  float length = std::sqrt(dx * dx + dy * dy);
  if (length < 0.001f)
    return;

  float nx = -dy / length * (thickness / 2.0f);
  float ny = dx / length * (thickness / 2.0f);

  SDL_Vertex verts[4];
  for (int i = 0; i < 4; ++i) {
    verts[i].color = color;
    verts[i].tex_coord = {0, 0};
  }

  // Use a slightly larger quad (half pixel) to help with anti-aliasing if MSAA
  // is off? Actually, MSAA is on, so we just need precise coordinates.
  verts[0].position = {x1 + nx, y1 + ny};
  verts[1].position = {x1 - nx, y1 - ny};
  verts[2].position = {x2 + nx, y2 + ny};
  verts[3].position = {x2 - nx, y2 - ny};

  int indices[] = {0, 1, 2, 1, 2, 3};
  SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);

  // Add rounded caps for smooth joins
  drawCircle(renderer, x1, y1, thickness / 2.0f, color);
  drawCircle(renderer, x2, y2, thickness / 2.0f, color);
}

void drawCircle(SDL_Renderer *renderer, float x, float y, float radius,
                SDL_Color color) {
  if (radius <= 0)
    return;

  // Adaptive segments: more for larger circles
  int segments = static_cast<int>(3.14159f * radius * 1.5f);
  if (segments < 16)
    segments = 16;
  if (segments > 64)
    segments = 64;

  std::vector<SDL_Vertex> verts;
  verts.reserve(segments + 2);

  SDL_Vertex center;
  center.position = {x, y};
  center.color = color;
  center.tex_coord = {0, 0};
  verts.push_back(center);

  for (int i = 0; i <= segments; ++i) {
    float theta = 2.0f * 3.1415926535f * static_cast<float>(i) /
                  static_cast<float>(segments);
    SDL_Vertex v;
    v.position = {x + radius * std::cos(theta), y + radius * std::sin(theta)};
    v.color = color;
    v.tex_coord = {0, 0};
    verts.push_back(v);
  }

  std::vector<int> indices;
  indices.reserve(segments * 3);
  for (int i = 1; i <= segments; ++i) {
    indices.push_back(0);
    indices.push_back(i);
    indices.push_back(i + 1);
  }

  SDL_RenderGeometry(renderer, nullptr, verts.data(),
                     static_cast<int>(verts.size()), indices.data(),
                     static_cast<int>(indices.size()));
}

void drawRect(SDL_Renderer *renderer, float x, float y, float w, float h,
              SDL_Color color) {
  SDL_Vertex verts[4];
  for (int i = 0; i < 4; ++i) {
    verts[i].color = color;
    verts[i].tex_coord = {0, 0};
  }
  verts[0].position = {x, y};
  verts[1].position = {x + w, y};
  verts[2].position = {x, y + h};
  verts[3].position = {x + w, y + h};

  int indices[] = {0, 1, 2, 1, 2, 3};
  SDL_RenderGeometry(renderer, nullptr, verts, 4, indices, 6);
}

void drawRectOutline(SDL_Renderer *renderer, float x, float y, float w, float h,
                     SDL_Color color) {
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  SDL_Rect r = {static_cast<int>(x), static_cast<int>(y), static_cast<int>(w),
                static_cast<int>(h)};
  SDL_RenderDrawRect(renderer, &r);
}

void drawCircleOutline(SDL_Renderer *renderer, float x, float y, float radius,
                       SDL_Color color) {
  if (radius <= 0)
    return;

  int segments = static_cast<int>(3.14159f * radius * 1.5f);
  if (segments < 16)
    segments = 16;
  if (segments > 64)
    segments = 64;

  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
  for (int i = 0; i < segments; ++i) {
    float t1 = 2.0f * 3.1415926535f * static_cast<float>(i) /
               static_cast<float>(segments);
    float t2 = 2.0f * 3.1415926535f * static_cast<float>(i + 1) /
               static_cast<float>(segments);
    SDL_RenderDrawLineF(renderer, x + radius * std::cos(t1),
                        y + radius * std::sin(t1), x + radius * std::cos(t2),
                        y + radius * std::sin(t2));
  }
}

void drawPolyline(SDL_Renderer *renderer, const SDL_FPoint *points, int count,
                  float thickness, SDL_Color color, bool closed) {
  if (count < 2)
    return;

  std::vector<SDL_Vertex> verts;
  std::vector<int> indices;

  float r = thickness / 2.0f;

  for (int i = 0; i < (closed ? count : count - 1); ++i) {
    int i1 = i;
    int i2 = (i + 1) % count;

    float dx = points[i2].x - points[i1].x;
    float dy = points[i2].y - points[i1].y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.001f)
      continue;

    float nx = -dy / len * r;
    float ny = dx / len * r;

    int base = static_cast<int>(verts.size());
    SDL_Vertex v[4];
    for (int j = 0; j < 4; ++j) {
      v[j].color = color;
      v[j].tex_coord = {0, 0};
    }

    v[0].position = {points[i1].x + nx, points[i1].y + ny};
    v[1].position = {points[i1].x - nx, points[i1].y - ny};
    v[2].position = {points[i2].x + nx, points[i2].y + ny};
    v[3].position = {points[i2].x - nx, points[i2].y - ny};

    verts.push_back(v[0]);
    verts.push_back(v[1]);
    verts.push_back(v[2]);
    verts.push_back(v[3]);

    indices.push_back(base + 0);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base + 3);

    // Draw caps at joints
    drawCircle(renderer, points[i1].x, points[i1].y, r, color);
    if (!closed && i == count - 2) {
      drawCircle(renderer, points[i2].x, points[i2].y, r, color);
    }
  }

  if (verts.empty())
    return;

  SDL_RenderGeometry(renderer, nullptr, verts.data(),
                     static_cast<int>(verts.size()), indices.data(),
                     static_cast<int>(indices.size()));
}

} // namespace RenderUtils
