#include "MapViewMenu.h"
#include "../core/Constants.h"
#include "../core/Theme.h"

MapViewMenu::MapViewMenu(FontManager &fontMgr)
    : Widget(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT),
      fontMgr_(fontMgr) {}

void MapViewMenu::show(AppConfig &config, std::function<void()> onApply) {
  config_ = &config;
  onApply_ = onApply;
  visible_ = true;

  // Make local copies for cancel support
  projection_ = config.projection;
  mapStyle_ = config.mapStyle;
  showGrid_ = config.showGrid;
  gridType_ = config.gridType;

  // Center the menu
  int menuW = 500;
  int menuH = 320; // Reduced height
  menuRect_ = {HamClock::LOGICAL_WIDTH / 2 - menuW / 2,
               HamClock::LOGICAL_HEIGHT / 2 - menuH / 2, menuW, menuH};

  // Layout sections
  int sectionX = menuRect_.x + 30;
  int btnW = 18; // Slightly smaller radio buttons
  int btnGap = 210;

  // Projection section
  int y = menuRect_.y + 45; // Start higher
  projHeaderY_ = y;
  y += 24; // Tighter vertical gap
  projEquiRect_ = {sectionX, y, btnW, btnW};
  projRobinsonRect_ = {sectionX + btnGap, y, btnW, btnW};

  // Style section
  y += 42;
  styleHeaderY_ = y;
  y += 24;
  styleNasaRect_ = {sectionX, y, btnW, btnW};
  styleTerrainRect_ = {sectionX + btnGap, y, btnW, btnW};
  y += 28;
  styleCountriesRect_ = {sectionX, y, btnW, btnW};

  // Grid section
  y += 42;
  gridHeaderY_ = y;
  y += 24;
  gridOffRect_ = {sectionX, y, btnW, btnW};
  gridLatLonRect_ = {sectionX + btnGap, y, btnW, btnW};
  y += 28;
  gridMaidenheadRect_ = {sectionX, y, btnW, btnW};

  // Footer buttons
  int btnFooterW = 100;
  int btnH = 32;
  int btnY = menuRect_.y + menuRect_.h - btnH - 18;
  cancelRect_ = {menuRect_.x + menuW / 2 - btnFooterW - 10, btnY, btnFooterW,
                 btnH};
  applyRect_ = {menuRect_.x + menuW / 2 + 10, btnY, btnFooterW, btnH};
}

void MapViewMenu::hide() { visible_ = false; }

void MapViewMenu::update() {}

void MapViewMenu::render(SDL_Renderer *renderer) {
  if (!visible_)
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Note: Background dimming is handled by main.cpp's centralized modal system

  // Menu background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_RenderFillRect(renderer, &menuRect_);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &menuRect_);

  // Title
  fontMgr_.drawText(renderer, "Map View Options", menuRect_.x + menuRect_.w / 2,
                    menuRect_.y + 20, themes.text, 16, false, true);

  // Projection Section
  fontMgr_.drawText(renderer, "Projection:", menuRect_.x + 25, projHeaderY_,
                    themes.text, 13, true);
  renderRadioButton(renderer, projEquiRect_, projection_ == "equirectangular",
                    "Equirectangular", themes.text);
  renderRadioButton(renderer, projRobinsonRect_, projection_ == "robinson",
                    "Robinson", themes.text);

  // Style Section
  fontMgr_.drawText(renderer, "Map Style:", menuRect_.x + 25, styleHeaderY_,
                    themes.text, 13, true);
  renderRadioButton(renderer, styleNasaRect_, mapStyle_ == "nasa",
                    "NASA Blue Marble", themes.text);

  // Terrain and Countries grayed out (not yet implemented)
  SDL_Color grayedText = {120, 120, 120, 255};
  renderRadioButton(renderer, styleTerrainRect_, mapStyle_ == "terrain",
                    "Terrain (coming soon)", grayedText);
  renderRadioButton(renderer, styleCountriesRect_, mapStyle_ == "countries",
                    "Countries (coming soon)", grayedText);

  // Grid Section
  fontMgr_.drawText(renderer, "Grid Overlay:", menuRect_.x + 25, gridHeaderY_,
                    themes.text, 13, true);
  renderRadioButton(renderer, gridOffRect_, !showGrid_, "Off", themes.text);
  renderRadioButton(renderer, gridLatLonRect_,
                    showGrid_ && gridType_ == "latlon", "Lat/Lon", themes.text);
  renderRadioButton(renderer, gridMaidenheadRect_,
                    showGrid_ && gridType_ == "maidenhead", "Maidenhead",
                    themes.text);

  // Footer Buttons
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Cancel button
  SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 120, 120, 130, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  fontMgr_.drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                    cancelRect_.y + cancelRect_.h / 2, {255, 255, 255, 255}, 14,
                    false, true);

  // Apply button
  SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                         themes.accent.b, 255);
  SDL_RenderFillRect(renderer, &applyRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &applyRect_);
  fontMgr_.drawText(renderer, "Apply", applyRect_.x + applyRect_.w / 2,
                    applyRect_.y + applyRect_.h / 2, {255, 255, 255, 255}, 14,
                    false, true);
}

void MapViewMenu::renderRadioButton(SDL_Renderer *renderer,
                                    const SDL_Rect &rect, bool selected,
                                    const std::string &label,
                                    const SDL_Color &textColor) {
  // Draw checkbox background (matching SetupScreen.cpp style)
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &rect);

  // Draw checkbox outline
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &rect);

  // Fill if selected
  if (selected) {
    SDL_SetRenderDrawColor(renderer, 0, 200, 100, 255);
    SDL_Rect check = {rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8};
    SDL_RenderFillRect(renderer, &check);
  }

  // Label - aligned to match SetupScreen.cpp style
  fontMgr_.drawText(renderer, label, rect.x + rect.w + 10, rect.y + 2,
                    textColor, 12, false, false);
}

bool MapViewMenu::onMouseUp(int mx, int my, Uint16) {
  if (!visible_)
    return false;

  SDL_Point pt = {mx, my};

  // Check projection buttons
  if (SDL_PointInRect(&pt, &projEquiRect_)) {
    projection_ = "equirectangular";
    return true;
  }
  if (SDL_PointInRect(&pt, &projRobinsonRect_)) {
    projection_ = "robinson";
    return true;
  }

  // Check style buttons (only NASA is active)
  if (SDL_PointInRect(&pt, &styleNasaRect_)) {
    mapStyle_ = "nasa";
    return true;
  }
  // Terrain and Countries are disabled for now
  // if (SDL_PointInRect(&pt, &styleTerrainRect_)) {
  //   mapStyle_ = "terrain";
  //   return true;
  // }
  // if (SDL_PointInRect(&pt, &styleCountriesRect_)) {
  //   mapStyle_ = "countries";
  //   return true;
  // }

  // Check grid buttons
  if (SDL_PointInRect(&pt, &gridOffRect_)) {
    showGrid_ = false;
    return true;
  }
  if (SDL_PointInRect(&pt, &gridLatLonRect_)) {
    showGrid_ = true;
    gridType_ = "latlon";
    return true;
  }
  if (SDL_PointInRect(&pt, &gridMaidenheadRect_)) {
    showGrid_ = true;
    gridType_ = "maidenhead";
    return true;
  }

  // Check Cancel button
  if (SDL_PointInRect(&pt, &cancelRect_)) {
    hide();
    return true;
  }

  // Check Apply button
  if (SDL_PointInRect(&pt, &applyRect_)) {
    // Apply changes to config
    config_->projection = projection_;
    config_->mapStyle = mapStyle_;
    config_->showGrid = showGrid_;
    config_->gridType = gridType_;

    hide();
    if (onApply_)
      onApply_();
    return true;
  }

  return true; // Consume all clicks when menu is visible
}

bool MapViewMenu::onKeyDown(SDL_Keycode key, Uint16) {
  if (!visible_)
    return false;

  if (key == SDLK_ESCAPE) {
    hide();
    return true;
  }

  return true; // Consume all keys when menu is visible
}
