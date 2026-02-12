#include "GimbalPanel.h"
#include "FontCatalog.h"
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GimbalPanel::GimbalPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void GimbalPanel::update() {
  if (predictor_) {
    predictor_->setObserver(obsLat_, obsLon_);
    auto pos = predictor_->observe();
    az_ = pos.azimuth;
    el_ = pos.elevation;
    hasSat_ = true;
  } else {
    hasSat_ = false;
  }
}

void GimbalPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Background
  SDL_SetRenderDrawColor(renderer, 20, 25, 25, 255); // Dark Slate tint
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &rect);

  if (!hasSat_) {
    fontMgr_.drawText(renderer, "No Sat Active", x_ + width_ / 2,
                      y_ + height_ / 2, {150, 150, 150, 255}, labelFontSize_,
                      false, true);
    return;
  }

  // Draw Sat Name
  fontMgr_.drawText(renderer, predictor_->satName(), x_ + width_ / 2, y_ + 10,
                    {0, 255, 0, 255}, labelFontSize_, true, true);

  // Draw Az/El values
  char buf[32];
  std::snprintf(buf, sizeof(buf), "AZ: %.1f", az_);
  fontMgr_.drawText(renderer, buf, 15 + x_, y_ + 35, {255, 255, 255, 255},
                    valueFontSize_);

  std::snprintf(buf, sizeof(buf), "EL: %.1f", el_);
  fontMgr_.drawText(renderer, buf, 15 + x_, y_ + 65, {255, 255, 255, 255},
                    valueFontSize_);

  // Graphical indicator (Mechanical Crosshair) - Center-aligned in the
  // remaining space
  int centerX = x_ + width_ / 2;
  int centerY = y_ + height_ - 50;
  int radius = 35;

  // Outer circle (simple octagonal approximation)
  SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
  for (int i = 0; i < 8; ++i) {
    double a1 = i * (45.0 * M_PI / 180.0);
    double a2 = (i + 1) * (45.0 * M_PI / 180.0);
    SDL_RenderDrawLine(renderer, centerX + radius * std::cos(a1),
                       centerY + radius * std::sin(a1),
                       centerX + radius * std::cos(a2),
                       centerY + radius * std::sin(a2));
  }

  // Crosshair
  SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
  SDL_RenderDrawLine(renderer, centerX - radius, centerY, centerX + radius,
                     centerY);
  SDL_RenderDrawLine(renderer, centerX, centerY - radius, centerX,
                     centerY + radius);

  // Azimuth indicator (North is 0, which is up/-Y in screen space)
  double azRad = (az_ - 90.0) * M_PI / 180.0;
  int tipX = centerX + static_cast<int>(std::cos(azRad) * radius);
  int tipY = centerY + static_cast<int>(std::sin(azRad) * radius);
  SDL_SetRenderDrawColor(renderer, 255, 128, 0, 255);
  SDL_RenderDrawLine(renderer, centerX, centerY, tipX, tipY);

  // Add a small arrow head
  // SDL_RenderFillRect(renderer, ...); // skip for simplicity

  // Elevation bar (Vertical on the right)
  int barW = 8;
  int barH = 60;
  int barX = x_ + width_ - 20;
  int barY = y_ + height_ - 80;
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_Rect barBg = {barX, barY, barW, barH};
  SDL_RenderFillRect(renderer, &barBg);

  if (el_ > -90) {
    // Map -90..90 to 0..barH.  el 0 is middle (barH/2).
    double normEl = (el_ + 90.0) / 180.0;
    int fillH = static_cast<int>(normEl * barH);
    SDL_Rect barFill = {barX, barY + barH - fillH, barW, 4}; // indicator line
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderFillRect(renderer, &barFill);

    // Horizontal tick for 0 degrees
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, barX - 2, barY + barH / 2, barX + barW + 2,
                       barY + barH / 2);
  }
}

void GimbalPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  valueFontSize_ = cat->ptSize(FontStyle::SmallBold);

  if (h < 100) {
    valueFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}
