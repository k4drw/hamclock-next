#pragma once

#include "Widget.h"
#include <SDL.h>
#include <functional>

namespace HamClock {

class ColorPicker : public Widget {
public:
  using ColorChangedCallback = std::function<void(SDL_Color)>;

  ColorPicker(int x, int y, int w, int h,
              ColorChangedCallback callback = nullptr);
  ~ColorPicker() override = default;

  void update() override {}
  void render(SDL_Renderer *renderer) override;
  bool onMouseDown(int mx, int my, Uint16 mod, int clicks) override;
  bool onMouseUp(int mx, int my, Uint16 mod, int clicks) override;
  void onMouseMove(int mx, int my) override;

  void setColor(SDL_Color color);
  SDL_Color getColor() const { return currentColor_; }

  void setOnColorChanged(ColorChangedCallback callback) {
    onColorChanged_ = callback;
  }

private:
  void updateHSVFromRGB();
  void updateRGBFromHSV();
  void notifyChanged();

  // Color state
  SDL_Color currentColor_;
  float h_ = 0.0f, s_ = 0.0f, v_ = 0.0f;

  ColorChangedCallback onColorChanged_;

  // UI state
  bool draggingHue_ = false;
  bool draggingSV_ = false;
  bool draggingSlider_ = false;
  int activeSlider_ = -1; // 0=R, 1=G, 2=B

  // Layout (internal metrics)
  SDL_Rect rectHue_;
  SDL_Rect rectSV_;
  SDL_Rect rectSliders_[3];
  SDL_Rect rectPreview_;

  void calculateLayout();
};

} // namespace HamClock
