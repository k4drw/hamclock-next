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
  
  virtual bool onRightClick(int mx, int my, Uint16 mod) {
    (void)mx;
    (void)my;
    (void)mod;
    return false;
  }

  // Called on mouse move. Check if handled.
  virtual void onMouseMove(int mx, int my) {
    (void)mx;
    (void)my;
  }

  virtual void onFontChanged() {}

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
  virtual void setLineAATexture(SDL_Texture *tex) { (void)tex; }
  virtual void setMetric(bool metric) { useMetric_ = metric; }

  // Semantic Debug API
  virtual std::string getName() const { return "Widget"; }
  virtual std::string getDisplayName() const { return ""; }
  virtual const char *typeId() const { return ""; }
  virtual bool isScrollable() const { return false; }
  virtual bool requiresConfigKey() const { return false; }
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

  // Called by DashboardContext after the main widget loop to draw the queued
  // tooltip (standard widgets) above all widget content.
  static void flushPendingTooltip(SDL_Renderer *renderer) {
    if (!s_pendingTooltip_.active || !s_pendingTooltip_.cat)
      return;
    s_pendingTooltip_.active = false;
    auto *cat = s_pendingTooltip_.cat;

    int tw1, th1, tw2 = 0, th2 = 0;
    cat->renderText(renderer, s_pendingTooltip_.text, s_pendingTooltip_.textColor,
                    FontStyle::Micro, &tw1, &th1);
    if (!s_pendingTooltip_.text2.empty())
      cat->renderText(renderer, s_pendingTooltip_.text2,
                      s_pendingTooltip_.textDimColor, FontStyle::Micro, &tw2, &th2);

    int padX = 8;
    int padY = 4;
    int lineGap = 2;
    int innerH = th1 + (th2 > 0 ? lineGap + th2 : 0);
    int boxW = std::max(tw1, tw2) + padX * 2;
    int boxH = innerH + padY * 2;

    int bx = s_pendingTooltip_.x - boxW / 2;
    int by = s_pendingTooltip_.y - boxH - 12;

    int screenW = 0, screenH = 0;
    SDL_GetRendererOutputSize(renderer, &screenW, &screenH);

    if (by < 0)
      by = s_pendingTooltip_.y + 16;
    if (bx < 0)
      bx = 0;
    if (bx + boxW > screenW)
      bx = screenW - boxW;

    SDL_Rect box = {bx, by, boxW, boxH};
    SDL_Rect fullScreen = {0, 0, screenW, screenH};
    SDL_RenderSetClipRect(renderer, &fullScreen);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, s_pendingTooltip_.bgColor.r,
                           s_pendingTooltip_.bgColor.g, s_pendingTooltip_.bgColor.b,
                           200);
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, s_pendingTooltip_.borderColor.r,
                           s_pendingTooltip_.borderColor.g,
                           s_pendingTooltip_.borderColor.b, 255);
    SDL_RenderDrawRect(renderer, &box);

    cat->drawText(renderer, s_pendingTooltip_.text, bx + padX, by + padY,
                  s_pendingTooltip_.textColor, FontStyle::Micro);
    if (!s_pendingTooltip_.text2.empty())
      cat->drawText(renderer, s_pendingTooltip_.text2, bx + padX,
                    by + padY + th1 + lineGap, s_pendingTooltip_.textDimColor,
                    FontStyle::Micro);

    SDL_RenderSetClipRect(renderer, nullptr);
  }

  // Override point for widgets with custom tooltip rendering (e.g. MapWidget).
  // Called by DashboardContext after flushPendingTooltip(). Default is no-op.
  virtual void renderTooltipLayer(SDL_Renderer *renderer) { (void)renderer; }

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

  // Deferred tooltip queue — populated by renderTooltip(), drawn by
  // flushPendingTooltip() after all widgets have rendered so the tooltip
  // always appears on top regardless of widget Z-order.
  struct PendingTooltip {
    bool active;
    std::string text;
    std::string text2;
    int x, y;
    SDL_Color textColor;
    SDL_Color textDimColor;
    SDL_Color bgColor;
    SDL_Color borderColor;
    FontCatalog *cat;
  };
  static inline PendingTooltip s_pendingTooltip_{};

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

  // Draw the standard panel title at (x_+10, y_+5) in accent/MicroBold.
  // Panels opt-in by calling this after renderChrome() in their render() method.
  void renderTitle(SDL_Renderer *renderer, FontManager &fontMgr,
                   const std::string &title) {
    ThemeColors themes = getThemeColors(theme_);
    auto *cat = fontMgr.catalog();
    if (cat)
      cat->drawText(renderer, title, x_ + 10, y_ + 5, themes.accent,
                    FontStyle::MicroBold);
  }

  // Queue this widget's active tooltip for deferred rendering.
  // Called during render(); actual drawing happens in flushPendingTooltip()
  // after all widgets have rendered, ensuring the tooltip is on top of every
  // widget's content regardless of rendering order.
  void renderTooltip(SDL_Renderer *renderer, FontManager &fontMgr) {
    (void)renderer;
    if (tooltip_.text.empty())
      return;
    // Auto-expire: if the stored mouse position is outside this widget's bounds,
    // the mouse has moved away — clear the tooltip rather than drawing it stale.
    if (tooltip_.x < x_ || tooltip_.x >= x_ + width_ ||
        tooltip_.y < y_ || tooltip_.y >= y_ + height_) {
      tooltip_.visible = false;
      tooltip_.text.clear();
      return;
    }
    ThemeColors themes = getThemeColors(theme_);
    s_pendingTooltip_ = {true,
                         tooltip_.text,
                         tooltip_.text2,
                         tooltip_.x,
                         tooltip_.y,
                         themes.text,
                         themes.textDim,
                         themes.bg,
                         themes.border,
                         fontMgr.catalog()};
  }

};
