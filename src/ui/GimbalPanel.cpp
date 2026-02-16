#include "GimbalPanel.h"
#include "FontCatalog.h"
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GimbalPanel::GimbalPanel(int x, int y, int w, int h, FontManager &fontMgr,
                         std::shared_ptr<RotatorDataStore> rotatorStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr),
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

  // Display status line (Rotator status or Sat name)
  if (hasRotator_) {
    // Show rotator status
    SDL_Color statusColor = rotatorConnected_ ? SDL_Color{0, 255, 0, 255}
                                               : SDL_Color{255, 128, 0, 255};
    const char *statusText =
        rotatorConnected_ ? "ROTATOR CONNECTED" : "ROTATOR OFFLINE";
    fontMgr_.drawText(renderer, statusText, x_ + width_ / 2, y_ + 10,
                      statusColor, labelFontSize_, true, true);
  } else if (hasSat_) {
    // Show satellite name (prediction mode)
    fontMgr_.drawText(renderer, predictor_->satName(), x_ + width_ / 2, y_ + 10,
                      {0, 255, 0, 255}, labelFontSize_, true, true);
  } else {
    // No data available
    fontMgr_.drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                      {150, 150, 150, 255}, labelFontSize_, false, true);
    return;
  }

  // Draw Az/El values
  char buf[64];
  std::snprintf(buf, sizeof(buf), "AZ: %.1f%c", az_,
                hasRotator_ ? '\xb0' : ' ');
  fontMgr_.drawText(renderer, buf, 15 + x_, y_ + 35, {255, 255, 255, 255},
                    valueFontSize_);

  std::snprintf(buf, sizeof(buf), "EL: %.1f%c", el_,
                hasRotator_ ? '\xb0' : ' ');
  fontMgr_.drawText(renderer, buf, 15 + x_, y_ + 60, {255, 255, 255, 255},
                    valueFontSize_);

  // Show data source indicator
  const char *sourceText =
      hasRotator_ ? "Live" : (hasSat_ ? "Predicted" : "---");
  SDL_Color sourceColor =
      hasRotator_ ? SDL_Color{0, 255, 255, 255} : SDL_Color{128, 128, 128, 255};
  fontMgr_.drawText(renderer, sourceText, 15 + x_, y_ + 85, sourceColor,
                    labelFontSize_);

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
    fontMgr_.drawText(renderer, buf, 15 + x_, y_ + 105,
                      {255, 200, 0, 255}, labelFontSize_ - 2);
  }

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
