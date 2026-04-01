#include "MapViewMenu.h"
#include "../core/Constants.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
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
  showBeacons_ = config.showBeacons;
  showBorders_ = config.showBorders;
  centerMapOnDe_ = config.centerMapOnDe;
  gridType_ = config.gridType;
  propOverlay_ = config.propOverlay;
  weatherOverlay_ = config.weatherOverlay;
  propBand_ = config.propBand;
  propMode_ = config.propMode;
  propPower_ = config.propPower;
  openCombo_ = -1;

  // Center the menu
  int menuW = 500;
  int menuH = 450;
  menuRect_ = {HamClock::LOGICAL_WIDTH / 2 - menuW / 2,
               HamClock::LOGICAL_HEIGHT / 2 - menuH / 2, menuW, menuH};

  int col1X = menuRect_.x + 20;
  int col2X = menuRect_.x + menuW / 2 + 10;
  int colW = menuW / 2 - 30; // 220

  int y = menuRect_.y + 45; // Start below title

  // Row 1
  projRec_ = {col1X, y + 25, colW, 30};
  styleRec_ = {col2X, y + 25, colW, 30};
  projHeaderY_ = y; // Label Y
  styleHeaderY_ = y;

  // Row 2
  y += 60;
  gridRec_ = {col1X, y + 25, colW, 30};
  overlayRec_ = {col2X, y + 25, colW, 30};
  gridHeaderY_ = y;
  mufRtHeaderY_ = y;

  // Row 3
  y += 60;
  weatherRec_ = {col1X, y + 25, colW, 30};
  weatherHeaderY_ = y;

  beaconsRec_ = {col2X + 10, y + 25, 20, 20};
  bordersRec_ = {col2X + 110, y + 25, 20, 20};
  centerDeCheckRect_ = {col2X + 10, y + 55, 20, 20};

  // Row 4 (VOACAP) - 3 columns
  y += 85;
  voacapHeaderY_ = y;
  int col3W = (menuW - 40) / 3 - 10; // ~143
  int c1 = menuRect_.x + 20;
  int c2 = c1 + col3W + 15;
  int c3 = c2 + col3W + 15;

  bandRec_ = {c1, y + 45, col3W, 30};
  modeRec_ = {c2, y + 45, col3W, 30};
  powerRec_ = {c3, y + 45, col3W, 30};

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

  auto *cat = fontMgr_.catalog();

  // Title
  cat->drawText(renderer, "Map View Options", menuRect_.x + menuRect_.w / 2,
                menuRect_.y + 15, themes.text, FontStyle::SmallBold, true,
                false, true);

  // Projection Section
  cat->drawText(renderer, "Projection", projRec_.x, projHeaderY_, themes.text,
                FontStyle::UI);
  std::string projLabel = "Equirectangular";
  if (projection_ == "robinson")
    projLabel = "Robinson";
  else if (projection_ == "azimuthal")
    projLabel = "Azimuthal";
  else if (projection_ == "mercator")
    projLabel = "Mercator";
  else if (projection_ == "dual_azimuthal")
    projLabel = "Dual Azimuthal";
  drawDropdown(renderer, projRec_, projLabel, openCombo_ == COMBO_PROJ);

  // Style Section
  cat->drawText(renderer, "Map Style", styleRec_.x, styleHeaderY_, themes.text,
                FontStyle::UI);
  std::string styleLabel = "NASA Blue Marble";
  if (mapStyle_ == "topo")
    styleLabel = "Topo";
  else if (mapStyle_ == "topo_bathy")
    styleLabel = "Topo + Bathy";
  drawDropdown(renderer, styleRec_, styleLabel, openCombo_ == COMBO_STYLE);

  // Grid Section
  cat->drawText(renderer, "Grid Overlay", gridRec_.x, gridHeaderY_, themes.text,
                FontStyle::UI);
  std::string gridLabel = "Off";
  if (showGrid_)
    gridLabel = (gridType_ == "maidenhead") ? "Maidenhead" : "Lat/Lon";
  drawDropdown(renderer, gridRec_, gridLabel, openCombo_ == COMBO_GRID);

  // Propagation Section
  cat->drawText(renderer, "Propagation Overlay", overlayRec_.x, mufRtHeaderY_,
                themes.text, FontStyle::UI);
  std::string propLabel = "None";
  if (propOverlay_ == PropOverlayType::Muf)
    propLabel = "MUF";
  else if (propOverlay_ == PropOverlayType::Voacap)
    propLabel = "VOACAP";
  else if (propOverlay_ == PropOverlayType::Reliability)
    propLabel = "Reliability";
  else if (propOverlay_ == PropOverlayType::Toa)
    propLabel = "TOA";
  else if (propOverlay_ == PropOverlayType::Heatmap)
    propLabel = "Heatmap";
  else if (propOverlay_ == PropOverlayType::Drap)
    propLabel = "DRAP";
  else if (propOverlay_ == PropOverlayType::Aurora)
    propLabel = "Aurora";
  drawDropdown(renderer, overlayRec_, propLabel, openCombo_ == COMBO_OVERLAY);

  // Weather Section
  cat->drawText(renderer, "Weather Overlay", weatherRec_.x, weatherHeaderY_,
                themes.text, FontStyle::UI);
  std::string weatherLabel = "None";
  if (weatherOverlay_ == WeatherOverlayType::WxMb)
    weatherLabel = "WX/Pressure";
  else if (weatherOverlay_ == WeatherOverlayType::CloudsGrib)
    weatherLabel = "Clouds (GFS)";
  drawDropdown(renderer, weatherRec_, weatherLabel,
               openCombo_ == COMBO_WEATHER);

  // Beacons & Borders Toggles
  renderRadioButton(renderer, beaconsRec_, showBeacons_, "Beacons",
                    themes.text);
  renderRadioButton(renderer, bordersRec_, showBorders_, "Borders",
                    themes.text);
  renderRadioButton(renderer, centerDeCheckRect_, centerMapOnDe_,
                    "Center on DE", themes.text);

  // VOACAP Extras (Used for VOACAP, Reliability, and TOA)
  if (propOverlay_ == PropOverlayType::Voacap ||
      propOverlay_ == PropOverlayType::Reliability ||
      propOverlay_ == PropOverlayType::Toa) {
    cat->drawText(renderer, "VOACAP Settings", menuRect_.x + 20, voacapHeaderY_,
                  themes.text, FontStyle::UI);
    // Draw separator line
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 80);
    SDL_RenderDrawLine(renderer, menuRect_.x + 20, voacapHeaderY_ + 18,
                       menuRect_.x + menuRect_.w - 20, voacapHeaderY_ + 18);

    cat->drawText(renderer, "Band", bandRec_.x, bandRec_.y - 20, themes.text,
                  FontStyle::UI);
    drawDropdown(renderer, bandRec_, propBand_, openCombo_ == COMBO_BAND);

    cat->drawText(renderer, "Mode", modeRec_.x, modeRec_.y - 20, themes.text,
                  FontStyle::UI);
    drawDropdown(renderer, modeRec_, propMode_, openCombo_ == COMBO_MODE);

    cat->drawText(renderer, "Power", powerRec_.x, powerRec_.y - 20, themes.text,
                  FontStyle::UI);
    drawDropdown(renderer, powerRec_, std::to_string(propPower_) + "W",
                 openCombo_ == COMBO_POWER);
  }


  // Footer Buttons
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Cancel button
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  cat->drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                cancelRect_.y + cancelRect_.h / 2, themes.bg,
                FontStyle::UI, true, false, true);

  // Apply button
  SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                         themes.rowStripe2.b, 255);
  SDL_RenderFillRect(renderer, &applyRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &applyRect_);
  cat->drawText(renderer, "Apply", applyRect_.x + applyRect_.w / 2,
                applyRect_.y + applyRect_.h / 2, themes.text,
                FontStyle::UI, true, false, true);

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
    else if (openCombo_ == COMBO_BAND)
      drawDropdownList(renderer, bandRec_, bandOpts_);
    else if (openCombo_ == COMBO_MODE)
      drawDropdownList(renderer, modeRec_, modeOpts_);
    else if (openCombo_ == COMBO_POWER)
      drawDropdownList(renderer, powerRec_, powerOpts_);
  }
}

bool MapViewMenu::onMouseDown(int mx, int my, Uint16 /*mod*/, int /*clicks*/) {
  if (!visible_)
    return false;
  SDL_Point pt = {mx, my};
  if (SDL_PointInRect(&pt, &menuRect_))
    return true;
  return false;
}

void MapViewMenu::drawDropdown(SDL_Renderer *renderer, const SDL_Rect &rect,
                               const std::string &currentVal, bool /*isOpen*/) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  auto *cat = fontMgr_.catalog();

  // Text
  cat->drawText(renderer, currentVal, rect.x + 10, rect.y + rect.h / 2, themes.text,
                FontStyle::Fast, false, false, true);

  // Arrow (solid filled triangle)
  int cx = rect.x + rect.w - 15;
  int cy = rect.y + rect.h / 2;
  RenderUtils::drawTriangle(renderer, (float)cx - 4, (float)cy - 2,
                            (float)cx + 4, (float)cy - 2, (float)cx,
                            (float)cy + 3, themes.text);
}

int MapViewMenu::getDropdownHeight(int numOpts) {
  return std::min(numOpts, maxVisibleItems_) * 30;
}

void MapViewMenu::drawDropdownList(SDL_Renderer *renderer,
                                   const SDL_Rect &headerRect,
                                   const std::vector<std::string> &opts) {
  ThemeColors themes = getThemeColors(theme_);
  int h = getDropdownHeight(opts.size());
  SDL_Rect listRect = {headerRect.x, headerRect.y + headerRect.h, headerRect.w,
                       h};

  auto *cat = fontMgr_.catalog();

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &listRect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &listRect);

  int visibleCount = std::min((int)opts.size(), maxVisibleItems_);
  for (int i = 0; i < visibleCount; ++i) {
    int idx = listScroll_ + i;
    if (idx >= (int)opts.size())
      break;

    SDL_Rect itemRec = {listRect.x, listRect.y + i * 30, listRect.w, 30};

    cat->drawText(renderer, opts[idx], itemRec.x + 10,
                  itemRec.y + itemRec.h / 2, themes.text,
                  FontStyle::Fast, false, false, true);

    // Divider
    if (i < visibleCount - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 100);
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

    SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g, themes.rowStripe2.b, 255);
    SDL_RenderFillRect(renderer, &track);

    SDL_Rect thumb = {track.x, thumbY, sbWidth, thumbH};
    SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g, themes.textDim.b, 255);
    SDL_RenderFillRect(renderer, &thumb);
  }
}

bool MapViewMenu::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!visible_)
    return false;
  SDL_Point pt = {mx, my};

  // Helper for combo handling
  auto handleCombo = [&](const SDL_Rect &rec, int comboId,
                         const std::vector<std::string> &opts,
                         std::function<void(int)> onSelect) {
    if (openCombo_ == comboId) {
      int h = getDropdownHeight(opts.size());
      SDL_Rect listRect = {rec.x, rec.y + rec.h, rec.w, h};
      if (SDL_PointInRect(&pt, &listRect)) {
        int visIdx = (pt.y - listRect.y) / 30;
        int idx = listScroll_ + visIdx;
        if (idx >= 0 && idx < (int)opts.size()) {
          onSelect(idx);
          openCombo_ = -1;
          return true;
        }
      }
      openCombo_ = -1;
      return true;
    }
    if (SDL_PointInRect(&pt, &rec)) {
      openCombo_ = (openCombo_ == comboId) ? -1 : comboId;
      listScroll_ = 0;
      return true;
    }
    return false;
  };

  if (handleCombo(projRec_, COMBO_PROJ, projOpts_, [&](int idx) {
        const char *p[] = {"equirectangular", "robinson", "azimuthal",
                           "mercator", "dual_azimuthal"};
        projection_ = p[idx];
      }))
    return true;

  if (handleCombo(styleRec_, COMBO_STYLE, mapOpts_, [&](int idx) {
        const char *s[] = {"nasa", "topo", "topo_bathy"};
        mapStyle_ = s[idx];
      }))
    return true;

  if (handleCombo(gridRec_, COMBO_GRID, gridOpts_, [&](int idx) {
        if (idx == 0) {
          showGrid_ = false;
        } else {
          showGrid_ = true;
          gridType_ = (idx == 1) ? "latlon" : "maidenhead";
        }
      }))
    return true;

  if (handleCombo(overlayRec_, COMBO_OVERLAY, overlayOpts_, [&](int idx) {
        PropOverlayType t[] = {
            PropOverlayType::None,        PropOverlayType::Muf,
            PropOverlayType::Voacap,       PropOverlayType::Reliability,
            PropOverlayType::Toa,          PropOverlayType::Heatmap,
            PropOverlayType::Drap,         PropOverlayType::Aurora};
        propOverlay_ = t[idx];
      }))
    return true;

  if (handleCombo(weatherRec_, COMBO_WEATHER, weatherOpts_, [&](int idx) {
        WeatherOverlayType t[] = {WeatherOverlayType::None,
                                  WeatherOverlayType::WxMb,
                                  WeatherOverlayType::CloudsGrib};
        weatherOverlay_ = t[idx];
      }))
    return true;

  // Toggles
  if (SDL_PointInRect(&pt, &beaconsRec_)) {
    showBeacons_ = !showBeacons_;
    return true;
  }
  if (SDL_PointInRect(&pt, &bordersRec_)) {
    showBorders_ = !showBorders_;
    return true;
  }
  if (SDL_PointInRect(&pt, &centerDeCheckRect_)) {
    centerMapOnDe_ = !centerMapOnDe_;
    return true;
  }

  // VOACAP Settings
  if (propOverlay_ == PropOverlayType::Voacap ||
      propOverlay_ == PropOverlayType::Reliability ||
      propOverlay_ == PropOverlayType::Toa) {
    if (handleCombo(bandRec_, COMBO_BAND, bandOpts_,
                    [&](int idx) { propBand_ = bandOpts_[idx]; }))
      return true;
    if (handleCombo(modeRec_, COMBO_MODE, modeOpts_,
                    [&](int idx) { propMode_ = modeOpts_[idx]; }))
      return true;
    if (handleCombo(powerRec_, COMBO_POWER, powerOpts_, [&](int idx) {
          propPower_ = StringUtils::safe_stoi(powerOpts_[idx]);
          if (propPower_ <= 0)
            propPower_ = 100;
        }))
      return true;
  }

  // Close any open combo if clicking elsewhere in menu
  if (openCombo_ != -1) {
    openCombo_ = -1;
    return true;
  }

  if (SDL_PointInRect(&pt, &cancelRect_)) {
    hide();
    return true;
  }
  if (SDL_PointInRect(&pt, &applyRect_)) {
    config_->projection = projection_;
    config_->mapStyle = mapStyle_;
    config_->showGrid = showGrid_;
    config_->showBeacons = showBeacons_;
    config_->showBorders = showBorders_;
    config_->gridType = gridType_;
    config_->propOverlay = propOverlay_;
    config_->weatherOverlay = weatherOverlay_;
    config_->propBand = propBand_;
    config_->propMode = propMode_;
    config_->propPower = propPower_;
    config_->centerMapOnDe = centerMapOnDe_;
    hide();
    if (onApply_)
      onApply_();
    return true;
  }

  return true; // Consume all clicks in menu
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
void MapViewMenu::renderRadioButton(SDL_Renderer *renderer, const SDL_Rect &rect,
                                    bool selected, const std::string &label,
                                    const SDL_Color &textColor) {
  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_);

  // Draw the outer square
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g, themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  // If selected, draw the inner "checked" square
  if (selected) {
    SDL_Rect check = {rect.x + 4, rect.y + 4, rect.w - 8, rect.h - 8};
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g, themes.success.b, 255);
    SDL_RenderFillRect(renderer, &check);
  }

  // Draw the label
  cat->drawText(renderer, label, rect.x + rect.w + 10, rect.y + rect.h / 2,
                textColor, FontStyle::UI, false, false, true);
}

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
