#pragma once

#include <SDL2/SDL.h>

class Widget {
public:
  Widget(int x, int y, int width, int height)
      : x_(x), y_(y), width_(width), height_(height) {}

  virtual ~Widget() = default;

  SDL_Rect getRect() const { return {x_, y_, width_, height_}; }

  virtual void update() = 0;
  virtual void render(SDL_Renderer *renderer) = 0;

  // Called by LayoutManager when the window is resized.
  virtual void onResize(int x, int y, int w, int h) {
    x_ = x;
    y_ = y;
    width_ = w;
    height_ = h;
  }

  // Called on mouse click. Returns true if the widget handled the event.
  virtual bool onMouseUp(int mx, int my, Uint16 mod) {
    (void)mx;
    (void)my;
    (void)mod;
    return false;
  }

  // Called on keyboard/text events. Returns true if consumed.
  virtual bool onKeyDown(SDL_Keycode key, Uint16 mod) {
    (void)key;
    (void)mod;
    return false;
  }
  virtual bool onTextInput(const char *text) {
    (void)text;
    return false;
  }
  virtual bool onMouseWheel(int scrollY) {
    (void)scrollY;
    return false;
  }

protected:
  int x_;
  int y_;
  int width_;
  int height_;
};
