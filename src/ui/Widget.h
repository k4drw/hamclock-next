#pragma once

#include "FontCatalog.h"
#include "FontManager.h"
#include "../core/Theme.h"
#include <SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>

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
  virtual bool onMouseDown(int mx, int my, Uint16 mod, int clicks) {
    (void)mx;
    (void)my;
    (void)mod;
    (void)clicks;
    return false;
  }

  virtual bool onMouseUp(int mx, int my, Uint16 mod, int clicks) {
    (void)mx;
    (void)my;
    (void)mod;
    (void)clicks;
    return false;
  }

  // Called on mouse move. Check if handled.
  virtual void onMouseMove(int mx, int my) {
    (void)mx;
    (void)my;
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

  virtual void setTheme(const std::string &theme) { theme_ = theme; }

  virtual bool isModalActive() const { return false; }
  virtual bool isConfiguring() const { return false; }
  virtual void renderModal(SDL_Renderer *renderer) { (void)renderer; }
  virtual void setMetric(bool metric) { useMetric_ = metric; }

  // Semantic Debug API
  virtual std::string getName() const { return "Widget"; }
  virtual std::string getDisplayName() const { return ""; }
  virtual std::vector<std::string> getActions() const { return {}; }
  virtual bool performAction(const std::string &action) {
    (void)action;
    return false;
  }

  virtual SDL_Rect getActionRect(const std::string &action) const {
    (void)action;
    return {0, 0, 0, 0};
  }
  virtual nlohmann::json getDebugData() const { return {}; }

protected:
  int x_;
  int y_;
  int width_;
  int height_;
  std::string theme_ = "default";
  bool useMetric_ = true;

  // Tooltip shared state
  struct Tooltip {
    bool visible = false;
    std::string text;
    std::string text2; // optional second line (dimmer, same font)
    int x = 0;
    int y = 0;
    uint32_t timestamp = 0;
    SDL_Texture *cachedTexture = nullptr;
    std::string cachedText;
    int cachedW = 0;
    int cachedH = 0;
  } tooltip_;

  // Draw the standard background fill + border rect for this widget.
  // Panels opt-in by calling this at the top of their render() method.
  void renderChrome(SDL_Renderer *renderer) {
    ThemeColors themes = getThemeColors(theme_);
    SDL_SetRenderDrawBlendMode(
        renderer,
        (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                           themes.bg.a);
    SDL_Rect rect = {x_, y_, width_, height_};
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, themes.border.a);
    SDL_RenderDrawRect(renderer, &rect);
  }

  // Default tooltip renderer — shared by all panels with simple tooltips.
  // Panels with custom tooltip logic (e.g. MapWidget) override this.
  // If tooltip_.text2 is set, it is drawn as a second dimmer line below text.
  void renderTooltip(SDL_Renderer *renderer, FontManager &fontMgr) {
    if (tooltip_.text.empty())
      return;

    ThemeColors themes = getThemeColors(theme_);
    auto *cat = fontMgr.catalog();
    int tw1, th1, tw2 = 0, th2 = 0;
    cat->renderText(renderer, tooltip_.text, themes.text, FontStyle::Micro,
                    &tw1, &th1);
    if (!tooltip_.text2.empty())
      cat->renderText(renderer, tooltip_.text2, themes.textDim, FontStyle::Micro,
                      &tw2, &th2);

    int padX = 8;
    int padY = 4;
    int lineGap = 2;
    int innerH = th1 + (th2 > 0 ? lineGap + th2 : 0);
    int boxW = std::max(tw1, tw2) + padX * 2;
    int boxH = innerH + padY * 2;

    int bx = tooltip_.x - boxW / 2;
    int by = tooltip_.y - boxH - 12;

    if (by < y_)
      by = tooltip_.y + 16;
    if (bx < x_)
      bx = x_;
    if (bx + boxW > x_ + width_)
      bx = x_ + width_ - boxW;

    SDL_Rect box = {bx, by, boxW, boxH};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 200);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &box);

    cat->drawText(renderer, tooltip_.text, bx + padX, by + padY, themes.text,
                  FontStyle::Micro);
    if (!tooltip_.text2.empty())
      cat->drawText(renderer, tooltip_.text2, bx + padX,
                    by + padY + th1 + lineGap, themes.textDim, FontStyle::Micro);
  }
};
