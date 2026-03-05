#include "GimbalPanel.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GimbalPanel::GimbalPanel(int x, int y, int w, int h, FontManager &fontMgr,
                         TextureManager &texMgr,
                         std::shared_ptr<RotatorDataStore> rotatorStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      rotatorStore_(std::move(rotatorStore)) {}

void GimbalPanel::update() {
  // Update satellite prediction
  if (predictor_) {
    predictor_->setObserver(obsLat_, obsLon_);
    auto pos = predictor_->observe();
    satAz_ = pos.azimuth;
    satEl_ = pos.elevation;
    hasSat_ = true;
  } else {
    hasSat_ = false;
  }

  // Update rotator position (real hardware data)
  if (rotatorStore_) {
    RotatorData rotData = rotatorStore_->get();
    hasRotator_ = rotData.valid;
    rotatorConnected_ = rotData.connected;

    // If we have real rotator data, use it for display
    // Otherwise fall back to satellite prediction
    if (hasRotator_) {
      az_ = rotData.azimuth;
      el_ = rotData.elevation;
    } else if (hasSat_) {
      // Fall back to satellite prediction
      az_ = satAz_;
      el_ = satEl_;
    }
  } else if (hasSat_) {
    // No rotator configured, use satellite prediction
    az_ = satAz_;
    el_ = satEl_;
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

  int titleH = 20;
  SDL_Color accent = {0, 200, 255, 255}; // Default cyan-ish accent
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "Rotator", x_ + 10, y_ + 5, accent,
                FontStyle::MicroBold);

  // Display status line (Rotator status or Sat name)
  int statusY = y_ + titleH + 5;
  if (hasRotator_) {
    // Show rotator status
    SDL_Color statusColor = rotatorConnected_ ? SDL_Color{0, 255, 0, 255}
                                              : SDL_Color{255, 128, 0, 255};
    const char *statusText =
        rotatorConnected_ ? "ROTATOR CONNECTED" : "ROTATOR OFFLINE";
    cat->drawText(renderer, statusText, x_ + width_ / 2, statusY, statusColor,
                  FontStyle::Fast, true);
  } else if (hasSat_) {
    // Show satellite name (prediction mode)
    cat->drawText(renderer, predictor_->satName(), x_ + width_ / 2, statusY,
                  {0, 255, 0, 255}, FontStyle::Fast, true);
  } else {
    // No data available
    cat->drawText(renderer, "No Data", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, {150, 150, 150, 255},
                  FontStyle::Fast, true);
    return;
  }

  // Draw Az/El values
  char buf[64];
  std::snprintf(buf, sizeof(buf), "AZ: %.1f%c", az_,
                hasRotator_ ? '\xb0' : ' ');
  cat->drawText(renderer, buf, 15 + x_, y_ + 35, {255, 255, 255, 255},
                FontStyle::SmallBold);

  std::snprintf(buf, sizeof(buf), "EL: %.1f%c", el_,
                hasRotator_ ? '\xb0' : ' ');
  cat->drawText(renderer, buf, 15 + x_, y_ + 60, {255, 255, 255, 255},
                FontStyle::SmallBold);

  // Show data source indicator
  const char *sourceText =
      hasRotator_ ? "Live" : (hasSat_ ? "Predicted" : "---");
  SDL_Color sourceColor =
      hasRotator_ ? SDL_Color{0, 255, 255, 255} : SDL_Color{128, 128, 128, 255};
  cat->drawText(renderer, sourceText, 15 + x_, y_ + 85, sourceColor,
                FontStyle::Fast);

  // If we have both rotator and satellite, show the difference
  if (hasRotator_ && hasSat_) {
    double azDiff = satAz_ - az_;
    double elDiff = satEl_ - el_;

    // Normalize azimuth difference to [-180, 180]
    while (azDiff > 180)
      azDiff -= 360;
    while (azDiff < -180)
      azDiff += 360;

    std::snprintf(buf, sizeof(buf), "Err: Az%.0f El%.0f", azDiff, elDiff);
    cat->drawText(renderer, buf, 15 + x_, y_ + 105, {255, 200, 0, 255},
                  FontStyle::Caption);
  }

  // Graphical indicator (Mechanical Crosshair) - Center-aligned in the
  // remaining space
  int centerX = x_ + width_ / 2;
  int centerY = y_ + height_ - 50;
  int radius = 35;

  // Outer circle (AA octagon approximation)
  SDL_Texture *lineTex = texMgr_.get("line_aa");
  {
    static const int kSegs = 48;
    SDL_FPoint pts[kSegs + 1];
    for (int i = 0; i <= kSegs; ++i) {
      double a = i * 2.0 * M_PI / kSegs;
      pts[i] = {(float)(centerX + radius * std::cos(a)),
                (float)(centerY + radius * std::sin(a))};
    }
    if (lineTex)
      RenderUtils::drawPolylineTextured(renderer, lineTex, pts, kSegs + 1, 1.2f,
                                        {60, 60, 60, 255}, false);
  }

  // Crosshair
  {
    SDL_FPoint hLine[] = {{(float)(centerX - radius), (float)centerY},
                          {(float)(centerX + radius), (float)centerY}};
    SDL_FPoint vLine[] = {{(float)centerX, (float)(centerY - radius)},
                          {(float)centerX, (float)(centerY + radius)}};
    if (lineTex) {
      RenderUtils::drawPolylineTextured(renderer, lineTex, hLine, 2, 1.0f,
                                        {50, 50, 50, 200});
      RenderUtils::drawPolylineTextured(renderer, lineTex, vLine, 2, 1.0f,
                                        {50, 50, 50, 200});
    }
  }

  // Azimuth indicator (North is 0, which is up/-Y in screen space)
  double azRad = (az_ - 90.0) * M_PI / 180.0;
  int tipX = centerX + static_cast<int>(std::cos(azRad) * radius);
  int tipY = centerY + static_cast<int>(std::sin(azRad) * radius);
  // Azimuth indicator needle (AA, bright orange)
  {
    SDL_FPoint needle[] = {{(float)centerX, (float)centerY},
                           {(float)tipX, (float)tipY}};
    if (lineTex)
      RenderUtils::drawPolylineTextured(renderer, lineTex, needle, 2, 2.0f,
                                        {255, 128, 0, 255});
  }

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

    // Horizontal tick for 0 degrees (AA)
    {
      SDL_FPoint tick[] = {
          {(float)barX - 2.0f, (float)barY + (float)barH * 0.5f},
          {(float)barX + (float)barW + 2.0f, (float)barY + (float)barH * 0.5f}};
      if (lineTex)
        RenderUtils::drawPolylineTextured(renderer, lineTex, tick, 2, 1.0f,
                                          {100, 100, 100, 200});
    }
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
