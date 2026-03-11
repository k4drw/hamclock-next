#include "GimbalPanel.h"
#include "../core/Theme.h"
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
      flipActive_ = rotData.flipActive;
    } else if (hasSat_) {
      // Fall back to satellite prediction
      az_ = satAz_;
      el_ = satEl_;
      flipActive_ = false;
    }
  } else if (hasSat_) {
    // No rotator configured, use satellite prediction
    az_ = satAz_;
    el_ = satEl_;
    flipActive_ = false;
  }
}

void GimbalPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int titleH = 20;
  auto *cat = fontMgr_.catalog();
  cat->drawText(renderer, "Rotator", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (flipActive_) {
    int upW = fontMgr_.getLogicalWidth("UPOVER", cat->ptSize(FontStyle::MicroBold));
    cat->drawText(renderer, "UPOVER", x_ + width_ - upW - 10, y_ + 5,
                  themes.warning, FontStyle::MicroBold);
  }

  // Display status line (Rotator status or Sat name)
  int statusY = y_ + titleH + 5;
  if (hasRotator_) {
    // Show rotator status
    SDL_Color statusColor = rotatorConnected_ ? themes.success : themes.warning;
    const char *statusText =
        rotatorConnected_ ? "ROTATOR CONNECTED" : "ROTATOR OFFLINE";
    cat->drawText(renderer, statusText, x_ + width_ / 2, statusY, statusColor,
                  FontStyle::Fast, true);
  } else if (hasSat_) {
    // Show satellite name (prediction mode)
    cat->drawText(renderer, predictor_->satName(), x_ + width_ / 2, statusY,
                  themes.success, FontStyle::Fast, true);
  } else {
    // No data available
    cat->drawText(renderer, "No Data", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, themes.textDim,
                  FontStyle::Fast, true);
    return;
  }

  // Draw Az/El values
  char buf[64];
  std::snprintf(buf, sizeof(buf), "AZ: %.1f%c", az_,
                hasRotator_ ? '\xb0' : ' ');
  cat->drawText(renderer, buf, 15 + x_, y_ + 35, themes.text,
                FontStyle::SmallBold);

  std::snprintf(buf, sizeof(buf), "EL: %.1f%c", el_,
                hasRotator_ ? '\xb0' : ' ');
  cat->drawText(renderer, buf, 15 + x_, y_ + 60, themes.text,
                FontStyle::SmallBold);

  // Show data source indicator
  const char *sourceText =
      hasRotator_ ? "Live" : (hasSat_ ? "Predicted" : "---");
  SDL_Color sourceColor =
      hasRotator_ ? themes.info : themes.textDim;
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
    cat->drawText(renderer, buf, 15 + x_, y_ + 105, themes.warning,
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
                                        themes.textDim, false);
  }

  // Crosshair
  {
    SDL_FPoint hLine[] = {{(float)(centerX - radius), (float)centerY},
                          {(float)(centerX + radius), (float)centerY}};
    SDL_FPoint vLine[] = {{(float)centerX, (float)(centerY - radius)},
                          {(float)centerX, (float)(centerY + radius)}};
    if (lineTex) {
      RenderUtils::drawPolylineTextured(renderer, lineTex, hLine, 2, 1.0f,
                                        themes.textDim);
      RenderUtils::drawPolylineTextured(renderer, lineTex, vLine, 2, 1.0f,
                                        themes.textDim);
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
                                        themes.accent);
  }

  // Elevation bar (Vertical on the right)
  int barW = 8;
  int barH = 60;
  int barX = x_ + width_ - 20;
  int barY = y_ + height_ - 80;
  SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g, themes.rowStripe2.b, 255);
  SDL_Rect barBg = {barX, barY, barW, barH};
  SDL_RenderFillRect(renderer, &barBg);

  if (el_ > -90) {
    // Map -90..90 to 0..barH.  el 0 is middle (barH/2).
    double normEl = (el_ + 90.0) / 180.0;
    int fillH = static_cast<int>(normEl * barH);
    SDL_Rect barFill = {barX, barY + barH - fillH, barW, 4}; // indicator line
    SDL_SetRenderDrawColor(renderer, themes.info.r, themes.info.g, themes.info.b, 255);
    SDL_RenderFillRect(renderer, &barFill);

    // Horizontal tick for 0 degrees (AA)
    {
      SDL_FPoint tick[] = {
          {(float)barX - 2.0f, (float)barY + (float)barH * 0.5f},
          {(float)barX + (float)barW + 2.0f, (float)barY + (float)barH * 0.5f}};
      if (lineTex)
        RenderUtils::drawPolylineTextured(renderer, lineTex, tick, 2, 1.0f,
                                          themes.textDim);
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
