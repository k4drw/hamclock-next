#include "ColorPicker.h"
#include "RenderUtils.h"
#include <algorithm>
#include <cmath>

namespace HamClock {

// Helper for HSV to RGB
static void hsv2rgb(float h, float s, float v, float &r, float &g, float &b) {
  if (s <= 0.0f) {
    r = g = b = v;
    return;
  }
  float hh = h;
  if (hh >= 360.0f)
    hh = 0.0f;
  hh /= 60.0f;
  int i = (int)hh;
  float ff = hh - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - (s * ff));
  float t = v * (1.0f - (s * (1.0f - ff)));

  switch (i) {
  case 0:
    r = v;
    g = t;
    b = p;
    break;
  case 1:
    r = q;
    g = v;
    b = p;
    break;
  case 2:
    r = p;
    g = v;
    b = t;
    break;
  case 3:
    r = p;
    g = q;
    b = v;
    break;
  case 4:
    r = t;
    g = p;
    b = v;
    break;
  case 5:
  default:
    r = v;
    g = p;
    b = q;
    break;
  }
}

// Helper for RGB to HSV
static void rgb2hsv(float r, float g, float b, float &h, float &s, float &v) {
  float min = std::min({r, g, b});
  float max = std::max({r, g, b});
  v = max;
  float delta = max - min;
  if (max > 0.0f) {
    s = delta / max;
  } else {
    s = 0.0f;
    h = NAN;
    return;
  }
  if (r >= max)
    h = (g - b) / delta;
  else if (g >= max)
    h = 2.0f + (b - r) / delta;
  else
    h = 4.0f + (r - g) / delta;
  h *= 60.0f;
  if (h < 0.0f)
    h += 360.0f;
}

ColorPicker::ColorPicker(int x, int y, int w, int h,
                         ColorChangedCallback callback)
    : Widget(x, y, w, h), onColorChanged_(callback) {
  currentColor_ = {255, 0, 0, 255};
  updateHSVFromRGB();
  calculateLayout();
}

void ColorPicker::calculateLayout() {
  // Hue strip on the right
  int hueWidth = 30;
  int padding = 10;

  rectHue_ = {x_ + width_ - hueWidth - padding, y_ + padding, hueWidth,
              height_ - padding * 2 - 60};

  // SV square on the left
  int svSize =
      std::min(width_ - hueWidth - padding * 3, height_ - padding * 2 - 60);
  rectSV_ = {x_ + padding, y_ + padding, svSize, svSize};

  // Sliders below
  int sliderY = y_ + height_ - 60;
  int sliderH = 15;
  for (int i = 0; i < 3; ++i) {
    rectSliders_[i] = {x_ + padding, sliderY + i * (sliderH + 2),
                       width_ - padding * 2 - 80, sliderH};
  }

  // Preview box
  rectPreview_ = {x_ + width_ - padding - 70, sliderY, 70, 50};
}

void ColorPicker::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  // 1. Draw SV Square
  for (int j = 0; j < rectSV_.h; ++j) {
    for (int i = 0; i < rectSV_.w; ++i) {
      float s = (float)i / (rectSV_.w - 1);
      float v = 1.0f - (float)j / (rectSV_.h - 1);
      float r, g, b;
      hsv2rgb(h_, s, v, r, g, b);
      SDL_SetRenderDrawColor(renderer, (Uint8)(r * 255), (Uint8)(g * 255),
                             (Uint8)(b * 255), 255);
      SDL_RenderDrawPoint(renderer, rectSV_.x + i, rectSV_.y + j);
    }
  }

  // Draw SV Crosshair
  int crossX = rectSV_.x + (int)(s_ * (rectSV_.w - 1));
  int crossY = rectSV_.y + (int)((1.0f - v_) * (rectSV_.h - 1));
  SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b,
                         255);
  SDL_RenderDrawLine(renderer, crossX - 5, crossY, crossX + 5, crossY);
  SDL_RenderDrawLine(renderer, crossX, crossY - 5, crossX, crossY + 5);

  // 2. Draw Hue Strip
  for (int j = 0; j < rectHue_.h; ++j) {
    float h = (float)j / (rectHue_.h - 1) * 360.0f;
    float r, g, b;
    hsv2rgb(h, 1.0f, 1.0f, r, g, b);
    SDL_SetRenderDrawColor(renderer, (Uint8)(r * 255), (Uint8)(g * 255),
                           (Uint8)(b * 255), 255);
    SDL_RenderDrawLine(renderer, rectHue_.x, rectHue_.y + j,
                       rectHue_.x + rectHue_.w - 1, rectHue_.y + j);
  }

  // Draw Hue Indicator
  int hueY = rectHue_.y + (int)(h_ / 360.0f * (rectHue_.h - 1));
  SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b,
                         255);
  SDL_Rect hueInd = {rectHue_.x - 2, hueY - 2, rectHue_.w + 4, 4};
  SDL_RenderDrawRect(renderer, &hueInd);

  // 3. Draw Sliders
  Uint8 parts[3] = {currentColor_.r, currentColor_.g, currentColor_.b};
  SDL_Color sliderColors[3] = {
      {255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255}};

  for (int i = 0; i < 3; ++i) {
    // Track
    SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                           themes.rowStripe1.b, 255);
    SDL_RenderFillRect(renderer, &rectSliders_[i]);

    // Fill
    SDL_Rect fill = rectSliders_[i];
    fill.w = (int)((float)parts[i] / 255.0f * rectSliders_[i].w);
    SDL_SetRenderDrawColor(renderer, sliderColors[i].r, sliderColors[i].g,
                           sliderColors[i].b, 255);
    SDL_RenderFillRect(renderer, &fill);

    // Border
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &rectSliders_[i]);
  }

  // 4. Draw Preview
  SDL_SetRenderDrawColor(renderer, currentColor_.r, currentColor_.g,
                         currentColor_.b, 255);
  SDL_RenderFillRect(renderer, &rectPreview_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rectPreview_);
}

bool ColorPicker::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  (void)mod;
  (void)clicks;
  if (mx >= rectHue_.x && mx < rectHue_.x + rectHue_.w && my >= rectHue_.y &&
      my < rectHue_.y + rectHue_.h) {
    draggingHue_ = true;
  } else if (mx >= rectSV_.x && mx < rectSV_.x + rectSV_.w && my >= rectSV_.y &&
             my < rectSV_.y + rectSV_.h) {
    draggingSV_ = true;
  } else {
    for (int i = 0; i < 3; ++i) {
      if (mx >= rectSliders_[i].x &&
          mx < rectSliders_[i].x + rectSliders_[i].w &&
          my >= rectSliders_[i].y &&
          my < rectSliders_[i].y + rectSliders_[i].h) {
        draggingSlider_ = true;
        activeSlider_ = i;
        break;
      }
    }
  }

  if (draggingHue_ || draggingSV_ || draggingSlider_) {
    onMouseMove(mx, my);
    return true;
  }
  return false;
}

bool ColorPicker::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  (void)mx;
  (void)my;
  (void)mod;
  (void)clicks;
  draggingHue_ = draggingSV_ = draggingSlider_ = false;
  activeSlider_ = -1;
  return false;
}

void ColorPicker::onMouseMove(int mx, int my) {
  if (draggingHue_) {
    h_ = std::clamp((float)(my - rectHue_.y) / (rectHue_.h - 1), 0.0f, 1.0f) *
         360.0f;
    updateRGBFromHSV();
    notifyChanged();
  } else if (draggingSV_) {
    s_ = std::clamp((float)(mx - rectSV_.x) / (rectSV_.w - 1), 0.0f, 1.0f);
    v_ = 1.0f -
         std::clamp((float)(my - rectSV_.y) / (rectSV_.h - 1), 0.0f, 1.0f);
    updateRGBFromHSV();
    notifyChanged();
  } else if (draggingSlider_) {
    Uint8 val =
        (Uint8)std::clamp((float)(mx - rectSliders_[activeSlider_].x) /
                              (rectSliders_[activeSlider_].w - 1) * 255.0f,
                          0.0f, 255.0f);
    if (activeSlider_ == 0)
      currentColor_.r = val;
    else if (activeSlider_ == 1)
      currentColor_.g = val;
    else if (activeSlider_ == 2)
      currentColor_.b = val;
    updateHSVFromRGB();
    notifyChanged();
  }
}

void ColorPicker::setColor(SDL_Color color) {
  currentColor_ = color;
  updateHSVFromRGB();
}

void ColorPicker::updateHSVFromRGB() {
  float r = currentColor_.r / 255.0f;
  float g = currentColor_.g / 255.0f;
  float b = currentColor_.b / 255.0f;
  rgb2hsv(r, g, b, h_, s_, v_);
}

void ColorPicker::updateRGBFromHSV() {
  float r, g, b;
  hsv2rgb(h_, s_, v_, r, g, b);
  currentColor_.r = (Uint8)(r * 255);
  currentColor_.g = (Uint8)(g * 255);
  currentColor_.b = (Uint8)(b * 255);
  currentColor_.a = 255;
}

void ColorPicker::notifyChanged() {
  if (onColorChanged_) {
    onColorChanged_(currentColor_);
  }
}

} // namespace HamClock
