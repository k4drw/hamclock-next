#include "MapViewMenu.h"
#include "../core/Constants.h"
#include "../core/Theme.h"
#include <string>

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
  propOverlay_ = config.propOverlay;
  weatherOverlay_ = config.weatherOverlay;
  propBand_ = config.propBand;
  propMode_ = config.propMode;
  propPower_ = config.propPower;
  openCombo_ = -1;

  // Center the menu
  int menuW = 500;
  int menuH = 410;
  menuRect_ = {HamClock::LOGICAL_WIDTH / 2 - menuW / 2,
               HamClock::LOGICAL_HEIGHT / 2 - menuH / 2, menuW, menuH};

  int col1X = menuRect_.x + 20;
  int col2X = menuRect_.x + menuW / 2 + 10;
  int colW = menuW / 2 - 30; // 220

  int y = menuRect_.y + 60; // Start below title

  // Row 1
  projRec_ = {col1X, y + 25, colW, 30};
  styleRec_ = {col2X, y + 25, colW, 30};
  projHeaderY_ = y; // Label Y
  styleHeaderY_ = y;

  // Row 2
  y += 70;
  gridRec_ = {col1X, y + 25, colW, 30};
  overlayRec_ = {col2X, y + 25, colW, 30};
  gridHeaderY_ = y;
  mufRtHeaderY_ = y;

  // Row 3
  y += 70;
  weatherRec_ = {col1X, y + 25, colW, 30};
  weatherHeaderY_ = y;

  // Row 4 (VOACAP) - 3 columns
  y += 70;
  int col3W = (menuW - 40) / 3 - 10; // ~143
  int c1 = menuRect_.x + 20;
  int c2 = c1 + col3W + 15;
  int c3 = c2 + col3W + 15;

  bandRec_ = {c1, y + 25, col3W, 30};
  modeRec_ = {c2, y + 25, col3W, 30};
  powerRec_ = {c3, y + 25, col3W, 30};

  // Footer buttons
  int btnFooterW = 100;
  int btnH = 34;
  int btnY = menuRect_.y + menuH - btnH - 15;
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
                    menuRect_.y + 20, themes.text, 18, false, true);

  // Projection Section
  fontMgr_.drawText(renderer, "Projection", projRec_.x, projHeaderY_,
                    themes.text, 16, false);
  std::string projLabel = "Equirectangular";
  if (projection_ == "robinson")
    projLabel = "Robinson";
  else if (projection_ == "mercator")
    projLabel = "Mercator";
  drawDropdown(renderer, projRec_, projLabel, openCombo_ == COMBO_PROJ);

  // Style Section
  fontMgr_.drawText(renderer, "Map Style", styleRec_.x, styleHeaderY_,
                    themes.text, 16, false);
  std::string styleLabel = "NASA Blue Marble";
  if (mapStyle_ == "topo")
    styleLabel = "Topo";
  else if (mapStyle_ == "topo_bathy")
    styleLabel = "Topo + Bathy";
  drawDropdown(renderer, styleRec_, styleLabel, openCombo_ == COMBO_STYLE);

  // Grid Section
  fontMgr_.drawText(renderer, "Grid Overlay", gridRec_.x, gridHeaderY_,
                    themes.text, 16, false);
  std::string gridLabel = "Off";
  if (showGrid_)
    gridLabel = (gridType_ == "maidenhead") ? "Maidenhead" : "Lat/Lon";
  drawDropdown(renderer, gridRec_, gridLabel, openCombo_ == COMBO_GRID);

  // Propagation Section
  fontMgr_.drawText(renderer, "Propagation Overlay", overlayRec_.x,
                    mufRtHeaderY_, themes.text, 16, false);
  std::string propLabel = "None";
  if (propOverlay_ == PropOverlayType::Muf)
    propLabel = "MUF";
  else if (propOverlay_ == PropOverlayType::Voacap)
    propLabel = "VOACAP";
  else if (propOverlay_ == PropOverlayType::Reliability)
    propLabel = "Reliability";
  else if (propOverlay_ == PropOverlayType::Toa)
    propLabel = "TOA";
  drawDropdown(renderer, overlayRec_, propLabel, openCombo_ == COMBO_OVERLAY);

  // Weather Section
  fontMgr_.drawText(renderer, "Weather Overlay", weatherRec_.x, weatherHeaderY_,
                    themes.text, 16, false);
  std::string weatherLabel = (weatherOverlay_ == WeatherOverlayType::Clouds)
                                 ? "Clouds"
                                 : "None";
  drawDropdown(renderer, weatherRec_, weatherLabel,
               openCombo_ == COMBO_WEATHER);

  // VOACAP Extras (Used for VOACAP, Reliability, and TOA)
  if (propOverlay_ == PropOverlayType::Voacap ||
      propOverlay_ == PropOverlayType::Reliability ||
      propOverlay_ == PropOverlayType::Toa) {
    fontMgr_.drawText(renderer, "Band", bandRec_.x, bandRec_.y - 20,
                      themes.text, 16, false);
    drawDropdown(renderer, bandRec_, propBand_, openCombo_ == COMBO_BAND);

    fontMgr_.drawText(renderer, "Mode", modeRec_.x, modeRec_.y - 20,
                      themes.text, 16, false);
    drawDropdown(renderer, modeRec_, propMode_, openCombo_ == COMBO_MODE);

    fontMgr_.drawText(renderer, "Power", powerRec_.x, powerRec_.y - 20,
                      themes.text, 16, false);
    drawDropdown(renderer, powerRec_, std::to_string(propPower_) + "W",
                 openCombo_ == COMBO_POWER);
  }

  // Draw open dropdown LIST on top of everything
  if (openCombo_ != -1) {
    if (openCombo_ == COMBO_PROJ)
      drawDropdownList(renderer, projRec_, projOpts_);
    else if (openCombo_ == COMBO_STYLE)
      drawDropdownList(renderer, styleRec_, mapOpts_);
    else if (openCombo_ == COMBO_GRID)
      drawDropdownList(renderer, gridRec_, gridOpts_);
          else if (openCombo_ == COMBO_OVERLAY)
            drawDropdownList(renderer, overlayRec_, overlayOpts_);
          else if (openCombo_ == COMBO_WEATHER)
            drawDropdownList(renderer, weatherRec_, weatherOpts_);
          else if (openCombo_ == COMBO_BAND)      drawDropdownList(renderer, bandRec_, bandOpts_);
    else if (openCombo_ == COMBO_MODE)
      drawDropdownList(renderer, modeRec_, modeOpts_);
    else if (openCombo_ == COMBO_POWER)
      drawDropdownList(renderer, powerRec_, powerOpts_);
  }

  // Footer Buttons
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Cancel button
  SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, 120, 120, 130, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  fontMgr_.drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                    cancelRect_.y + cancelRect_.h / 2, {255, 255, 255, 255}, 16,
                    false, true);

  // Apply button
  SDL_SetRenderDrawColor(renderer, 0, 100, 200, 255);
  SDL_RenderFillRect(renderer, &applyRect_);
  SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255);
  SDL_RenderDrawRect(renderer, &applyRect_);
  fontMgr_.drawText(renderer, "Apply", applyRect_.x + applyRect_.w / 2,
                    applyRect_.y + applyRect_.h / 2, {255, 255, 255, 255}, 16,
                    false, true);
}

void MapViewMenu::drawDropdown(SDL_Renderer *renderer, const SDL_Rect &rect,
                               const std::string &currentVal, bool isOpen) {
  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &rect);

  // Text
  SDL_Color txtCol = {220, 220, 220, 255};
  fontMgr_.drawText(renderer, currentVal, rect.x + 10, rect.y + 4, txtCol, 14,
                    false);

  // Arrow
  int cx = rect.x + rect.w - 15;
  int cy = rect.y + rect.h / 2;
  SDL_RenderDrawLine(renderer, cx - 4, cy - 2, cx + 4, cy - 2);
  SDL_RenderDrawLine(renderer, cx - 4, cy - 2, cx, cy + 3);
  SDL_RenderDrawLine(renderer, cx, cy + 3, cx + 4, cy - 2);
}

int MapViewMenu::getDropdownHeight(int numOpts) {
  return std::min(numOpts, maxVisibleItems_) * 30;
}

void MapViewMenu::drawDropdownList(SDL_Renderer *renderer,
                                   const SDL_Rect &headerRect,
                                   const std::vector<std::string> &opts) {
  int h = getDropdownHeight(opts.size());
  SDL_Rect listRect = {headerRect.x, headerRect.y + headerRect.h, headerRect.w,
                       h};

  // Background
  SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
  SDL_RenderFillRect(renderer, &listRect);
  SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
  SDL_RenderDrawRect(renderer, &listRect);

  int visibleCount = std::min((int)opts.size(), maxVisibleItems_);
  for (int i = 0; i < visibleCount; ++i) {
    int idx = listScroll_ + i;
    if (idx >= (int)opts.size())
      break;

    SDL_Rect itemRec = {listRect.x, listRect.y + i * 30, listRect.w, 30};

    fontMgr_.drawText(renderer, opts[idx], itemRec.x + 10, itemRec.y + 4,
                      {255, 255, 255, 255}, 14, false);

    // Divider
    if (i < visibleCount - 1) {
      SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
      SDL_RenderDrawLine(renderer, itemRec.x, itemRec.y + 29,
                         itemRec.x + itemRec.w, itemRec.y + 29);
    }
  }

  // Scrollbar if needed
  if ((int)opts.size() > maxVisibleItems_) {
    int sbWidth = 6;
    int trackH = listRect.h - 4;
    SDL_Rect track = {listRect.x + listRect.w - sbWidth - 2, listRect.y + 2,
                      sbWidth, trackH};

    // Thumb
    float ratio = (float)maxVisibleItems_ / opts.size();
    int thumbH = std::max(10, (int)(trackH * ratio));
    int scrollableItems = (int)opts.size() - maxVisibleItems_;
    float scrollPct =
        (scrollableItems > 0) ? (float)listScroll_ / scrollableItems : 0.0f;
    int thumbY = track.y + (int)(scrollPct * (trackH - thumbH));

    SDL_SetRenderDrawColor(renderer, 60, 60, 70, 255);
    SDL_RenderFillRect(renderer, &track);

    SDL_Rect thumb = {track.x, thumbY, sbWidth, thumbH};
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderFillRect(renderer, &thumb);
  }
}

bool MapViewMenu::onMouseUp(int mx, int my, Uint16) {
  if (!visible_)
    return false;
  SDL_Point pt = {mx, my};

  // Check Dropdowns
  auto handleCombo = [&](const SDL_Rect &rec, int comboId,
                         const std::vector<std::string> &opts,
                         std::function<void(int)> onSelect) {
    if (openCombo_ == comboId) {
      // Check clicks inside list
      int h = getDropdownHeight(opts.size());
      SDL_Rect listRect = {rec.x, rec.y + rec.h, rec.w, h};
      if (SDL_PointInRect(&pt, &listRect)) {
        int visIdx = (pt.y - listRect.y) / 30; // visual index
        int idx = listScroll_ + visIdx;
        if (idx >= 0 && idx < (int)opts.size()) {
          onSelect(idx);
          openCombo_ = -1;
          return true;
        }
      }
      // Clicked outside? Close it
      openCombo_ = -1;
      return true;
    }

    // Click on Header? Toggle
    if (SDL_PointInRect(&pt, &rec)) {
      if (openCombo_ == -1) {
        openCombo_ = comboId;
        listScroll_ = 0; // Reset scroll
      } else {
        openCombo_ = -1;
      }
      return true;
    }
    return false;
  };

  // Use a helper macro or just direct calls
  if (handleCombo(projRec_, COMBO_PROJ, projOpts_, [&](int idx) {
        if (idx == 0)
          projection_ = "equirectangular";
        else if (idx == 1)
          projection_ = "robinson";
        else if (idx == 2)
          projection_ = "mercator";
      }))
    return true;

  if (handleCombo(styleRec_, COMBO_STYLE, mapOpts_, [&](int idx) {
        if (idx == 0)
          mapStyle_ = "nasa";
        else if (idx == 1)
          mapStyle_ = "topo";
        else if (idx == 2)
          mapStyle_ = "topo_bathy";
      }))
    return true;

  if (handleCombo(gridRec_, COMBO_GRID, gridOpts_, [&](int idx) {
        if (idx == 0)
          showGrid_ = false;
        else if (idx == 1) {
          showGrid_ = true;
          gridType_ = "latlon";
        } else if (idx == 2) {
          showGrid_ = true;
          gridType_ = "maidenhead";
        }
      }))
    return true;

          if (handleCombo(overlayRec_, COMBO_OVERLAY, overlayOpts_, [&](int idx) {
                if (idx == 0)
                  propOverlay_ = PropOverlayType::None;
                else if (idx == 1)
                  propOverlay_ = PropOverlayType::Muf;
                else if (idx == 2)
                  propOverlay_ = PropOverlayType::Voacap;
                else if (idx == 3)
                  propOverlay_ = PropOverlayType::Reliability;
                else if (idx == 4)
                  propOverlay_ = PropOverlayType::Toa;
              }))
            return true;
      
          if (handleCombo(weatherRec_, COMBO_WEATHER, weatherOpts_, [&](int idx) {
                if (idx == 0)
                  weatherOverlay_ = WeatherOverlayType::None;
                else if (idx == 1)
                  weatherOverlay_ = WeatherOverlayType::Clouds;
              }))
            return true;
      
          if (propOverlay_ == PropOverlayType::Voacap ||          propOverlay_ == PropOverlayType::Reliability ||
          propOverlay_ == PropOverlayType::Toa) {
        if (handleCombo(bandRec_, COMBO_BAND, bandOpts_,                    [&](int idx) { propBand_ = bandOpts_[idx]; }))
      return true;

    if (handleCombo(modeRec_, COMBO_MODE, modeOpts_,
                    [&](int idx) { propMode_ = modeOpts_[idx]; }))
      return true;

    if (handleCombo(powerRec_, COMBO_POWER, powerOpts_, [&](int idx) {
          // parse "100W" -> 100
          try {
            propPower_ = std::stoi(powerOpts_[idx]);
          } catch (...) {
            propPower_ = 100;
          }
        }))
      return true;
  }

  // If we clicked anywhere else while a combo is open, close it
  if (openCombo_ != -1) {
    openCombo_ = -1;
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
          config_->propOverlay = propOverlay_;
          config_->weatherOverlay = weatherOverlay_;
          config_->propBand = propBand_;    config_->propMode = propMode_;
    config_->propPower = propPower_;

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
    if (openCombo_ != -1)
      openCombo_ = -1;
    else
      hide();
    return true;
  }
  return true;
}

// Dummy helper just in case
void MapViewMenu::renderRadioButton(SDL_Renderer *, const SDL_Rect &, bool,
                                    const std::string &, const SDL_Color &) {}

bool MapViewMenu::onMouseWheel(int scrollY) {
  if (!visible_ || openCombo_ == -1)
    return false;

  // Determine which list is open
  int totalItems = 0;
  switch (openCombo_) {
  case COMBO_PROJ:
    totalItems = projOpts_.size();
    break;
  case COMBO_STYLE:
    totalItems = mapOpts_.size();
    break;
  case COMBO_GRID:
    totalItems = gridOpts_.size();
    break;
  case COMBO_OVERLAY:
    totalItems = overlayOpts_.size();
    break;
  case COMBO_BAND:
    totalItems = bandOpts_.size();
    break;
  case COMBO_MODE:
    totalItems = modeOpts_.size();
    break;
  case COMBO_POWER:
    totalItems = powerOpts_.size();
    break;
  }

  if (totalItems <= maxVisibleItems_)
    return true; // Consume but do nothing

  listScroll_ -= scrollY; // Scroll up (neg) -> decrease index
  if (listScroll_ < 0)
    listScroll_ = 0;
  if (listScroll_ > totalItems - maxVisibleItems_)
    listScroll_ = totalItems - maxVisibleItems_;

  return true;
}
