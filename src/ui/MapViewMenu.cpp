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
  showLocalPropGauge_ = config.showLocalPropGauge;
  centerMapOnDe_ = config.centerMapOnDe;
  gridType_ = config.gridType;
  propOverlay_ = config.propOverlay;
  weatherOverlay_ = config.weatherOverlay;
  propBand_ = config.propBand;
  propMode_ = config.propMode;
  propPower_ = config.propPower;
  propToa_ = config.propToa;
  propPath_ = config.propPath;
  propAntGain_ = config.propAntGain;
  propColormap_ = config.propColormap;

  propRotation_ = config.propRotation;
  rotatingProp_ = !propRotation_.empty();

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
  propToaInput_.setValue(buf);
  propToaInput_.setActive(false);

  std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
  propAntGainInput_.setValue(buf);
  propAntGainInput_.setActive(false);

  openCombo_ = -1;

  // Initialize customizer sub-modal
  propCustomizer_ = std::make_unique<HamClock::PropColorCustomizer>(
      HamClock::LOGICAL_WIDTH / 2 - 250, HamClock::LOGICAL_HEIGHT / 2 - 190, 500, 380,
      fontMgr_, config.theme, config.colorOverrides, [onApply]() { onApply(); });

  recalcLayout();
}

void MapViewMenu::recalcLayout() {
  // Center the menu
  int menuW = 500;
  int menuH = 480;
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
  propColormapRec_ = {col2X, y + 25, colW - 50, 30};
  editPropColorsRec_ = {col2X + colW - 45, y + 25, 45, 30};
  propColorHeaderY_ = y;

  // Row 4 (Toggles)
  y += 70;
  auto *cat = fontMgr_.catalog();
  int beaconsLabelW =
      fontMgr_.getLogicalWidth("Beacons", cat->ptSize(FontStyle::UI));
  beaconsRec_ = {col1X + 10, y, 20 + 10 + beaconsLabelW, 20};
  int bordersLabelW =
      fontMgr_.getLogicalWidth("Borders", cat->ptSize(FontStyle::UI));
  bordersRec_ = {col1X + 110, y, 20 + 10 + bordersLabelW, 20};
  int propGaugeLabelW =
      fontMgr_.getLogicalWidth("Prop Gauge", cat->ptSize(FontStyle::UI));
  localPropGaugeRec_ = {col2X + 10, y, 20 + 10 + propGaugeLabelW, 20};
  int centerDeLabelW =
      fontMgr_.getLogicalWidth("Center on DE", cat->ptSize(FontStyle::UI));
  centerDeCheckRect_ = {col2X + 130, y, 20 + 10 + centerDeLabelW, 20};

  // Row 5 (VOACAP) - 3 columns
  y += 35;
  voacapHeaderY_ = y;
  int col3W = (menuW - 40) / 3;
  int c1 = menuRect_.x + 20;
  int c2 = c1 + col3W + 10;
  int c3 = c2 + col3W + 10;

  bandRec_ = {c1, y + 35, col3W, 30};
  modeRec_ = {c2, y + 35, col3W, 30};
  powerRec_ = {c3, y + 35, col3W, 30};

  // Row 5b (TOA, Path stacked, Ant Gain)
  y += 90;
  toaRec_ = {c1, y + 10, 80, 26};
  toaUpRec_ = {c1 + 82, y + 10, 24, 12};
  toaDnRec_ = {c1 + 82, y + 24, 24, 12};

  // Path radios stacked vertically under c2
  int spLabelW =
      fontMgr_.getLogicalWidth("Short", cat->ptSize(FontStyle::Fast));
  spRec_ = {c2, y + 8, 16 + 10 + spLabelW, 16};
  int lpLabelW =
      fontMgr_.getLogicalWidth("Long", cat->ptSize(FontStyle::Fast));
  lpRec_ = {c2, y + 30, 16 + 10 + lpLabelW, 16};

  // Ant Gain spinner at c3 (same geometry as TOA)
  antGainRec_ = {c3, y + 10, 80, 26};
  antGainUpRec_ = {c3 + 82, y + 10, 24, 12};
  antGainDnRec_ = {c3 + 82, y + 24, 24, 12};

  // Rotation checklist rects (2 columns)
  propRotationHeaderY_ = voacapHeaderY_;
  propRotationCheckRects_.clear();

  for (int i = 0; i < 6; ++i) { // Only 6 now
    int row = i % 3;
    int col = i / 3;
    int cx = menuRect_.x + 30 + col * (menuW / 2 - 20);
    int cy = propRotationHeaderY_ + 25 + row * 26;
    propRotationCheckRects_.push_back({cx, cy, menuW / 2 - 40, 20});
  }

  // Footer buttons
  int btnFooterW = 100;
  int btnH = 34;
  int btnY = menuRect_.y + menuH - btnH - 15;
  cancelRect_ = {menuRect_.x + menuW / 2 - btnFooterW - 10, btnY, btnFooterW,
                 btnH};
  applyRect_ = {menuRect_.x + menuW / 2 + 10, btnY, btnFooterW, btnH};
}

void MapViewMenu::onFontChanged() {
    recalcLayout();
}

void MapViewMenu::hide() { visible_ = false; }

void MapViewMenu::update() {
  if (!visible_)
    return;

  if (propCustomizer_ && propCustomizer_->isActive()) {
    propCustomizer_->update();
    return;
  }

  if (repeatDir_ == 0)
    return;

  uint32_t now = SDL_GetTicks();
  if (now - repeatStartMs_ > 500) {  // Initial delay
    if (now - repeatLastMs_ > 50) {  // Repeat interval
      char buf[32];
      if (repeatTarget_ == 0) {
        propToa_ += (repeatDir_ * 1.0f);
        if (propToa_ < 0.0f) propToa_ = 0.0f;
        if (propToa_ > 90.0f) propToa_ = 90.0f;
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
        propToaInput_.setValue(buf);
      } else {
        propAntGain_ = std::max(-10, std::min(30, propAntGain_ + repeatDir_));
        std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
        propAntGainInput_.setValue(buf);
      }
      repeatLastMs_ = now;
    }
  }
}

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
    propLabel = "MUF-RT";
  else if (propOverlay_ == PropOverlayType::Voacap)
    propLabel = "VOACAP";
  else if (propOverlay_ == PropOverlayType::Reliability)
    propLabel = "Reliability";
  else if (propOverlay_ == PropOverlayType::Heatmap)
    propLabel = "Heatmap";
  else if (propOverlay_ == PropOverlayType::Drap)
    propLabel = "DRAP";
  else if (propOverlay_ == PropOverlayType::Aurora)
    propLabel = "Aurora";
  
  if (rotatingProp_)
    propLabel = "Rotating...";

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

  // Prop Color Section
  cat->drawText(renderer, "Prop Colors", propColormapRec_.x, propColorHeaderY_,
                themes.text, FontStyle::UI);
  std::string propColorLabel = "Muted";
  if (propColormap_ == "vibrant")
    propColorLabel = "Vibrant";
  else if (propColormap_ == "custom")
    propColorLabel = "Custom";
    
  drawDropdown(renderer, propColormapRec_, propColorLabel,
               openCombo_ == COMBO_PROP_COLOR);

  if (propColormap_ == "custom") {
    SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g, themes.accent.b, 255);
    SDL_RenderFillRect(renderer, &editPropColorsRec_);
    cat->drawText(renderer, "Edit...", editPropColorsRec_.x + editPropColorsRec_.w / 2,
                  editPropColorsRec_.y + editPropColorsRec_.h / 2, themes.bg,
                  FontStyle::UI, true, false, true);
  }

  // Beacons & Borders Toggles
  renderRadioButton(renderer, beaconsRec_, showBeacons_, "Beacons",
                    themes.text);
  renderRadioButton(renderer, bordersRec_, showBorders_, "Borders",
                    themes.text);
  renderRadioButton(renderer, centerDeCheckRect_, centerMapOnDe_,
                    "Center on DE", themes.text);
  renderRadioButton(renderer, localPropGaugeRec_, showLocalPropGauge_,
                    "Prop Gauge", themes.text);

  // VOACAP Extras (Used for VOACAP and Reliability)
  if (propOverlay_ == PropOverlayType::Voacap ||
      propOverlay_ == PropOverlayType::Reliability) {
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

    // Row 5: TOA and Path
    cat->drawText(renderer, "Take-Off Angle (deg)", toaRec_.x, toaRec_.y - 20,
                  themes.text, FontStyle::Fast);
    propToaInput_.render(renderer, fontMgr_, toaRec_.x, toaRec_.y, toaRec_.w,
                         toaRec_.h, FontStyle::UI, 5, propToaInput_.isActive(),
                         true, themes.accent, themes.border, themes.text,
                         themes.text, themes.textDim);

    // Up/Down Arrows for TOA
    SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                           themes.rowStripe2.b, 255);
    SDL_RenderFillRect(renderer, &toaUpRec_);
    SDL_RenderFillRect(renderer, &toaDnRec_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &toaUpRec_);
    SDL_RenderDrawRect(renderer, &toaDnRec_);

    RenderUtils::drawTriangle(
        renderer, toaUpRec_.x + 4, toaUpRec_.y + toaUpRec_.h - 4,
        toaUpRec_.x + toaUpRec_.w - 4, toaUpRec_.y + toaUpRec_.h - 4,
        toaUpRec_.x + toaUpRec_.w / 2, toaUpRec_.y + 4, themes.text);

    RenderUtils::drawTriangle(renderer, toaDnRec_.x + 4, toaDnRec_.y + 4,
                              toaDnRec_.x + toaDnRec_.w - 4, toaDnRec_.y + 4,
                              toaDnRec_.x + toaDnRec_.w / 2,
                              toaDnRec_.y + toaDnRec_.h - 4, themes.text);

    cat->drawText(renderer, "Path", spRec_.x, toaRec_.y - 20, themes.text,
                  FontStyle::Fast);
    renderRadioButton(renderer, spRec_, propPath_ == 0, "Short", themes.text);
    renderRadioButton(renderer, lpRec_, propPath_ == 1, "Long", themes.text);

    // Antenna Gain spinner (mirrors TOA)
    cat->drawText(renderer, "Ant Gain (dBi)", antGainRec_.x, antGainRec_.y - 20,
                  themes.text, FontStyle::Fast);
    propAntGainInput_.render(renderer, fontMgr_, antGainRec_.x, antGainRec_.y,
                             antGainRec_.w, antGainRec_.h, FontStyle::UI, 5,
                             propAntGainInput_.isActive(), true, themes.accent,
                             themes.border, themes.text, themes.text,
                             themes.textDim);

    SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                           themes.rowStripe2.b, 255);
    SDL_RenderFillRect(renderer, &antGainUpRec_);
    SDL_RenderFillRect(renderer, &antGainDnRec_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &antGainUpRec_);
    SDL_RenderDrawRect(renderer, &antGainDnRec_);
    RenderUtils::drawTriangle(
        renderer, antGainUpRec_.x + 4, antGainUpRec_.y + antGainUpRec_.h - 4,
        antGainUpRec_.x + antGainUpRec_.w - 4, antGainUpRec_.y + antGainUpRec_.h - 4,
        antGainUpRec_.x + antGainUpRec_.w / 2, antGainUpRec_.y + 4, themes.text);
    RenderUtils::drawTriangle(
        renderer, antGainDnRec_.x + 4, antGainDnRec_.y + 4,
        antGainDnRec_.x + antGainDnRec_.w - 4, antGainDnRec_.y + 4,
        antGainDnRec_.x + antGainDnRec_.w / 2, antGainDnRec_.y + antGainDnRec_.h - 4,
        themes.text);
  } else if (rotatingProp_) {
    // Render Rotation Checklist
    cat->drawText(renderer, "Rotation Set", menuRect_.x + 20, propRotationHeaderY_,
                  themes.text, FontStyle::UI);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, 80);
    SDL_RenderDrawLine(renderer, menuRect_.x + 20, propRotationHeaderY_ + 18,
                       menuRect_.x + menuRect_.w - 20, propRotationHeaderY_ + 18);

    PropOverlayType types[] = {
        PropOverlayType::Muf,  PropOverlayType::Voacap, PropOverlayType::Reliability,
        PropOverlayType::Heatmap, PropOverlayType::Drap, PropOverlayType::Aurora};
    const char* labels[] = {"MUF-RT", "VOACAP", "Reliability", "Heatmap", "DRAP", "Aurora"};

    for (int i = 0; i < 6; ++i) {
      bool isSelected = std::find(propRotation_.begin(), propRotation_.end(), types[i]) != propRotation_.end();
      renderRadioButton(renderer, propRotationCheckRects_[i], isSelected, labels[i], themes.text);
    }
  }

  // Footer Buttons
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  // Cancel button
  SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g,
                         themes.danger.b, 255);
  SDL_RenderFillRect(renderer, &cancelRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &cancelRect_);
  cat->drawText(renderer, "Cancel", cancelRect_.x + cancelRect_.w / 2,
                cancelRect_.y + cancelRect_.h / 2, themes.bg, FontStyle::UI,
                true, false, true);

  // Apply button
  SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                         themes.success.b, 255);
  SDL_RenderFillRect(renderer, &applyRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &applyRect_);
  cat->drawText(renderer, "Apply", applyRect_.x + applyRect_.w / 2,
                applyRect_.y + applyRect_.h / 2, themes.text, FontStyle::UI,
                true, false, true);

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
    else if (openCombo_ == COMBO_PROP_COLOR)
      drawDropdownList(renderer, propColormapRec_, propColorOpts_);
  }

  // Customizer sub-modal
  if (propCustomizer_ && propCustomizer_->isActive()) {
    propCustomizer_->render(renderer);
  }
}

bool MapViewMenu::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (!visible_)
    return false;

  if (propCustomizer_ && propCustomizer_->isActive()) {
    return propCustomizer_->onMouseDown(mx, my, mod, clicks);
  }

  SDL_Point pt = {mx, my};

  if (SDL_PointInRect(&pt, &toaRec_)) {
    if (!propToaInput_.isActive()) {
      propAntGainInput_.setActive(false);
      propToaInput_.setActive(true);
      SDL_StartTextInput();
    }
    propToaInput_.onMouseDown(mx, my, clicks, fontMgr_, toaRec_.x, toaRec_.y,
                              toaRec_.w, toaRec_.h, FontStyle::UI, 5);
    return true;
  }

  if (SDL_PointInRect(&pt, &antGainRec_)) {
    if (!propAntGainInput_.isActive()) {
      propToaInput_.setActive(false);
      propAntGainInput_.setActive(true);
      SDL_StartTextInput();
    }
    propAntGainInput_.onMouseDown(mx, my, clicks, fontMgr_, antGainRec_.x,
                                  antGainRec_.y, antGainRec_.w, antGainRec_.h,
                                  FontStyle::UI, 5);
    return true;
  }

  auto commitInputs = [&]() {
    if (propToaInput_.isActive()) {
      propToaInput_.setActive(false);
      SDL_StopTextInput();
      try {
        propToa_ = std::round(std::stof(propToaInput_.getValue()));
        if (propToa_ < 0.0f) propToa_ = 0.0f;
        if (propToa_ > 90.0f) propToa_ = 90.0f;
      } catch (...) {}
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
      propToaInput_.setValue(buf);
    }
    if (propAntGainInput_.isActive()) {
      propAntGainInput_.setActive(false);
      SDL_StopTextInput();
      try {
        propAntGain_ = std::stoi(propAntGainInput_.getValue());
        if (propAntGain_ < -10) propAntGain_ = -10;
        if (propAntGain_ > 30)  propAntGain_ = 30;
      } catch (...) {}
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
      propAntGainInput_.setValue(buf);
    }
  };
  commitInputs();

  if (SDL_PointInRect(&pt, &toaUpRec_)) {
    repeatDir_ = 1; repeatTarget_ = 0;
    repeatStartMs_ = repeatLastMs_ = SDL_GetTicks();
    propToa_ += 1.0f;
    if (propToa_ > 90.0f) propToa_ = 90.0f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
    propToaInput_.setValue(buf);
    return true;
  }
  if (SDL_PointInRect(&pt, &toaDnRec_)) {
    repeatDir_ = -1; repeatTarget_ = 0;
    repeatStartMs_ = repeatLastMs_ = SDL_GetTicks();
    propToa_ -= 1.0f;
    if (propToa_ < 0.0f) propToa_ = 0.0f;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
    propToaInput_.setValue(buf);
    return true;
  }

  if (SDL_PointInRect(&pt, &antGainUpRec_)) {
    repeatDir_ = 1; repeatTarget_ = 1;
    repeatStartMs_ = repeatLastMs_ = SDL_GetTicks();
    propAntGain_ = std::min(30, propAntGain_ + 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
    propAntGainInput_.setValue(buf);
    return true;
  }
  if (SDL_PointInRect(&pt, &antGainDnRec_)) {
    repeatDir_ = -1; repeatTarget_ = 1;
    repeatStartMs_ = repeatLastMs_ = SDL_GetTicks();
    propAntGain_ = std::max(-10, propAntGain_ - 1);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
    propAntGainInput_.setValue(buf);
    return true;
  }

  if (SDL_PointInRect(&pt, &menuRect_))
    return true;
  return false;
}

void MapViewMenu::drawDropdown(SDL_Renderer *renderer, const SDL_Rect &rect,
                               const std::string &currentVal, bool /*isOpen*/) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &rect);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &rect);

  auto *cat = fontMgr_.catalog();

  // Text
  cat->drawText(renderer, currentVal, rect.x + 10, rect.y + rect.h / 2,
                themes.text, FontStyle::Fast, false, false, true);

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
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &listRect);

  int visibleCount = std::min((int)opts.size(), maxVisibleItems_);
  for (int i = 0; i < visibleCount; ++i) {
    int idx = listScroll_ + i;
    if (idx >= (int)opts.size())
      break;

    SDL_Rect itemRec = {listRect.x, listRect.y + i * 30, listRect.w, 30};

    cat->drawText(renderer, opts[idx], itemRec.x + 10,
                  itemRec.y + itemRec.h / 2, themes.text, FontStyle::Fast,
                  false, false, true);

    // Divider
    if (i < visibleCount - 1) {
      SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 100);
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

    SDL_SetRenderDrawColor(renderer, themes.rowStripe2.r, themes.rowStripe2.g,
                           themes.rowStripe2.b, 255);
    SDL_RenderFillRect(renderer, &track);

    SDL_Rect thumb = {track.x, thumbY, sbWidth, thumbH};
    SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g,
                           themes.textDim.b, 255);
    SDL_RenderFillRect(renderer, &thumb);
  }
}

bool MapViewMenu::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (!visible_)
    return false;

  if (propCustomizer_ && propCustomizer_->isActive()) {
    return propCustomizer_->onMouseUp(mx, my, mod, clicks);
  }

  // Edit Colors button
  if (propColormap_ == "custom" && mx >= editPropColorsRec_.x &&
      mx < editPropColorsRec_.x + editPropColorsRec_.w &&
      my >= editPropColorsRec_.y &&
      my < editPropColorsRec_.y + editPropColorsRec_.h) {
    propCustomizer_->setActive(true);
    return true;
  }

  repeatDir_ = 0;
  propToaInput_.onMouseUp();
  propAntGainInput_.onMouseUp();

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
            PropOverlayType::None,   PropOverlayType::Muf,
            PropOverlayType::Voacap, PropOverlayType::Reliability,
            PropOverlayType::Heatmap, PropOverlayType::Drap,
            PropOverlayType::Aurora};
        if (idx < 7) {
          propOverlay_ = t[idx];
          rotatingProp_ = false;
          propRotation_.clear();
        } else {
          // "Rotating..." selected
          rotatingProp_ = true;
          if (propRotation_.empty()) {
            // Default to current overlay if it was valid
            if (propOverlay_ != PropOverlayType::None)
              propRotation_.push_back(propOverlay_);
            else
              propRotation_.push_back(PropOverlayType::Muf);
          }
        }
      }))
    return true;

  if (handleCombo(weatherRec_, COMBO_WEATHER, weatherOpts_, [&](int idx) {
        WeatherOverlayType t[] = {WeatherOverlayType::None,
                                  WeatherOverlayType::WxMb,
                                  WeatherOverlayType::CloudsGrib};
        weatherOverlay_ = t[idx];
      }))
    return true;

  if (handleCombo(propColormapRec_, COMBO_PROP_COLOR, propColorOpts_, [&](int idx) {
        if (idx == 0) propColormap_ = "vibrant";
        else if (idx == 1) propColormap_ = "muted";
        else if (idx == 2) propColormap_ = "custom";
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
  if (SDL_PointInRect(&pt, &localPropGaugeRec_)) {
    showLocalPropGauge_ = !showLocalPropGauge_;
    return true;
  }

  // VOACAP Settings
  if (propOverlay_ == PropOverlayType::Voacap ||
      propOverlay_ == PropOverlayType::Reliability) {
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

    if (SDL_PointInRect(&pt, &spRec_)) {
      propPath_ = 0;
      return true;
    }
      if (SDL_PointInRect(&pt, &lpRec_)) {
        propPath_ = 1;
        return true;
      }
    } else if (rotatingProp_) {
      PropOverlayType types[] = {
          PropOverlayType::Muf,  PropOverlayType::Voacap, PropOverlayType::Reliability,
          PropOverlayType::Heatmap, PropOverlayType::Drap, PropOverlayType::Aurora};
      for (int i = 0; i < 6; ++i) {
        if (SDL_PointInRect(&pt, &propRotationCheckRects_[i])) {
          auto it = std::find(propRotation_.begin(), propRotation_.end(), types[i]);
          if (it != propRotation_.end()) {
            if (propRotation_.size() > 1)
              propRotation_.erase(it);
          } else {
            propRotation_.push_back(types[i]);
          }
          return true;
        }
      }
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
    config_->showLocalPropGauge = showLocalPropGauge_;
    config_->centerMapOnDe = centerMapOnDe_;
    config_->gridType = gridType_;
    config_->propOverlay = propOverlay_;
    config_->weatherOverlay = weatherOverlay_;
    config_->propBand = propBand_;
    config_->propMode = propMode_;
    config_->propPower = propPower_;
    config_->propToa = propToa_;
    config_->propPath = propPath_;
    config_->propAntGain = propAntGain_;
    config_->propColormap = propColormap_;
    config_->centerMapOnDe = centerMapOnDe_;
    config_->propRotation = propRotation_;
    hide();
    if (onApply_)
      onApply_();
    return true;
  }

  return true;  // Consume all clicks in menu
}

bool MapViewMenu::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (!visible_)
    return false;

  if (propToaInput_.isActive()) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      propToaInput_.setActive(false);
      SDL_StopTextInput();
      try {
        propToa_ = std::round(std::stof(propToaInput_.getValue()));
        if (propToa_ < 0.0f) propToa_ = 0.0f;
        if (propToa_ > 90.0f) propToa_ = 90.0f;
      } catch (...) {}
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(propToa_));
      propToaInput_.setValue(buf);
      return true;
    }
    return propToaInput_.onKeyDown(key, mod);
  }

  if (propAntGainInput_.isActive()) {
    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
      propAntGainInput_.setActive(false);
      SDL_StopTextInput();
      try {
        propAntGain_ = std::stoi(propAntGainInput_.getValue());
        if (propAntGain_ < -10) propAntGain_ = -10;
        if (propAntGain_ > 30)  propAntGain_ = 30;
      } catch (...) {}
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%d", propAntGain_);
      propAntGainInput_.setValue(buf);
      return true;
    }
    return propAntGainInput_.onKeyDown(key, mod);
  }

  if (key == SDLK_ESCAPE) {
    if (openCombo_ != -1)
      openCombo_ = -1;
    else
      hide();
    return true;
  }
  return true;
}

bool MapViewMenu::onTextInput(const char *text) {
  if (!visible_)
    return false;
  return false;
}

void MapViewMenu::renderRadioButton(SDL_Renderer *renderer,
                                    const SDL_Rect &rect, bool selected,
                                    const std::string &label,
                                    const SDL_Color &textColor) {
  auto *cat = fontMgr_.catalog();
  ThemeColors themes = getThemeColors(theme_);

  // Draw the outer square (fixed size based on rect.h)
  SDL_Rect box = {rect.x, rect.y, rect.h, rect.h};
  SDL_SetRenderDrawColor(renderer, themes.rowStripe1.r, themes.rowStripe1.g,
                         themes.rowStripe1.b, 255);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &box);

  // If selected, draw the inner "checked" square
  if (selected) {
    SDL_Rect check = {box.x + 4, box.y + 4, box.w - 8, box.h - 8};
    SDL_SetRenderDrawColor(renderer, themes.success.r, themes.success.g,
                           themes.success.b, 255);
    SDL_RenderFillRect(renderer, &check);
  }

  // Draw the label
  cat->drawText(renderer, label, box.x + box.w + 10, box.y + box.h / 2,
                textColor, FontStyle::UI, false, false, true);
}

void MapViewMenu::onMouseMove(int mx, int my) {
  if (visible_ && propCustomizer_ && propCustomizer_->isActive()) {
    propCustomizer_->onMouseMove(mx, my);
  }
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
    case COMBO_PROP_COLOR:
      totalItems = propColorOpts_.size();
      break;
  }

  if (totalItems <= maxVisibleItems_)
    return true;  // Consume but do nothing

  listScroll_ -= scrollY;  // Scroll up (neg) -> decrease index
  if (listScroll_ < 0)
    listScroll_ = 0;
  if (listScroll_ > totalItems - maxVisibleItems_)
    listScroll_ = totalItems - maxVisibleItems_;

  return true;
}
