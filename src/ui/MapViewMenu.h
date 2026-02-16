#pragma once

#include "../core/ConfigManager.h"
#include "FontManager.h"
#include "Widget.h"

#include <SDL.h>

#include <functional>
#include <string>

class MapViewMenu : public Widget {
public:
  MapViewMenu(FontManager &fontMgr);

  void show(AppConfig &config, std::function<void()> onApply);
  void hide();
  bool isVisible() const { return visible_; }

  void update() override;
  void render(SDL_Renderer *renderer) override;
  bool onMouseUp(int mx, int my, Uint16 mod) override;
  bool onKeyDown(SDL_Keycode key, Uint16 mod) override;

  void setTheme(const std::string &theme) override { theme_ = theme; }

private:
  FontManager &fontMgr_;
  bool visible_ = false;
  AppConfig *config_ = nullptr;
  std::function<void()> onApply_;

  // Local copy of settings (for cancel support)
  std::string projection_;
  std::string mapStyle_;
  bool showGrid_;
  std::string gridType_;

  // Menu layout
  SDL_Rect menuRect_;
  SDL_Rect projEquiRect_, projRobinsonRect_;
  SDL_Rect styleNasaRect_, styleTerrainRect_, styleCountriesRect_;
  SDL_Rect gridOffRect_, gridLatLonRect_, gridMaidenheadRect_;
  SDL_Rect applyRect_, cancelRect_;
  int projHeaderY_ = 0, styleHeaderY_ = 0, gridHeaderY_ = 0;

  std::string theme_ = "default";

  void renderRadioButton(SDL_Renderer *renderer, const SDL_Rect &rect,
                         bool selected, const std::string &label,
                         const SDL_Color &textColor);
};
