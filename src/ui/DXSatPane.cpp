#include "DXSatPane.h"
#include "../core/Constants.h"
#include "../core/GreylineCalculator.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cmath>

DXSatPane::DXSatPane(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr,
                     std::shared_ptr<HamClockState> state,
                     SatelliteManager &satMgr,
                     std::shared_ptr<WeatherStore> weatherStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), state_(state),
      satMgr_(satMgr),
      dxPanel_(x, y, w, h, fontMgr, state, std::move(weatherStore)),
      satPanel_(x, y, w, h, fontMgr, texMgr),
      satelliteSetup_(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT,
                      fontMgr, satMgr),
      greylineModal_(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT,
                     fontMgr) {

  dxPanel_.setOnGreylineSync([this]() {
    auto window = HamClock::GreylineCalculator::findNextOverlap(
        state_->deLocation, state_->dxLocation,
        std::chrono::system_clock::now());
    greylineModal_.setWindow(window, state_->dxCallsign);
  });
}

DXSatPane::~DXSatPane() { destroyMenuTextures(); }

void DXSatPane::renderModal(SDL_Renderer *renderer) {
  if (greylineModal_.isActive()) {
    greylineModal_.render(renderer);
  } else if (satelliteSetup_.isActive()) {
    satelliteSetup_.render(renderer);
  } else if (menuState_ != MenuState::Closed) {
    renderMenu(renderer);
  }
}

void DXSatPane::setObserver(double latDeg, double lonDeg) {
  predictor_.setObserver(latDeg, lonDeg);
}

OrbitPredictor *DXSatPane::activePredictor() {
  if (mode_ == Mode::SAT && predictor_.isReady())
    return &predictor_;
  return nullptr;
}

void DXSatPane::restoreState(const std::string &panelMode,
                             const std::string &satName) {
  selectedSatName_ = satName;
  if (panelMode == "sat" && !satName.empty()) {
    // Try to load satellite now; if data isn't available yet, defer
    auto tle = satMgr_.findByName(satName);
    if (tle && predictor_.loadTLE(*tle)) {
      selectedSatName_ = tle->name;
      satPanel_.setPredictor(&predictor_);
      mode_ = Mode::SAT;
    } else {
      pendingSatRestore_ = satName;
    }
  }
}

void DXSatPane::notifyModeChanged() {
  if (onModeChanged_) {
    std::string modeStr = (mode_ == Mode::SAT) ? "sat" : "dx";
    onModeChanged_(modeStr, selectedSatName_);
  }
}

// --- Widget overrides ---

void DXSatPane::update() {
  // Deferred satellite restore: retry once data arrives
  if (!pendingSatRestore_.empty() && satMgr_.hasData()) {
    auto tle = satMgr_.findByName(pendingSatRestore_);
    if (tle && predictor_.loadTLE(*tle)) {
      selectedSatName_ = tle->name;
      satPanel_.setPredictor(&predictor_);
      mode_ = Mode::SAT;
    }
    pendingSatRestore_.clear();
  }

  if (menuState_ != MenuState::Closed || satelliteSetup_.isActive())
    return;

  if (mode_ == Mode::DX) {
    dxPanel_.update();
  } else {
    satPanel_.update();
  }
}

void DXSatPane::render(SDL_Renderer *renderer) {
  if (mode_ == Mode::DX) {
    dxPanel_.render(renderer);
  } else {
    satPanel_.render(renderer);
  }

  if (mode_ == Mode::SAT && !isModalActive()) {
    ThemeColors themes = getThemeColors(theme_);
    int headerH = std::max(1, height_ / 10);

    // Reserve space for PaneContainer's maximize button (mirrors its btnSz formula)
    int maxBtnReserve = std::max(8, std::min(14, std::min(width_, height_) / 6)) + 4;

    // Rotator "Trk" button (offset left to clear maximize button)
    trackButtonRect_ = {x_ + width_ - headerH - maxBtnReserve, y_, headerH, headerH};
    bool isTracking = (satMgr_.getTrackedSatellite() == selectedSatName_);
    SDL_Color rotBg = isTracking ? themes.success : themes.rowStripe1;

    SDL_SetRenderDrawColor(renderer, rotBg.r, rotBg.g, rotBg.b, 255);
    SDL_RenderFillRect(renderer, &trackButtonRect_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &trackButtonRect_);
    fontMgr_.catalog()->drawText(renderer, "Trk",
                                 trackButtonRect_.x + trackButtonRect_.w / 2,
                                 trackButtonRect_.y + trackButtonRect_.h / 2,
                                 isTracking ? themes.bg : themes.text, FontStyle::Tiny, true, false, true);

    // Map "Pth" button (to the left of Trk): toggles satellite ground track
    mapTrackBtnRect_ = {x_ + width_ - 2 * headerH - 2 - maxBtnReserve, y_, headerH, headerH};
    SDL_Color pathBg = mapTrackVisible_ ? themes.accent : themes.rowStripe1;

    SDL_SetRenderDrawColor(renderer, pathBg.r, pathBg.g, pathBg.b, 255);
    SDL_RenderFillRect(renderer, &mapTrackBtnRect_);
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
    SDL_RenderDrawRect(renderer, &mapTrackBtnRect_);
    fontMgr_.catalog()->drawText(renderer, "Pth",
                                 mapTrackBtnRect_.x + mapTrackBtnRect_.w / 2,
                                 mapTrackBtnRect_.y + mapTrackBtnRect_.h / 2,
                                 mapTrackVisible_ ? themes.bg : themes.text, FontStyle::Tiny, true, false, true);
  }
}

void DXSatPane::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  dxPanel_.onResize(x, y, w, h);
  satPanel_.onResize(x, y, w, h);
  satelliteSetup_.onResize(x, y, w, h);
  menuFontSize_ = fontMgr_.catalog()->ptSize(FontStyle::Fast);
  if (menuState_ != MenuState::Closed)
    destroyMenuTextures();
}

bool DXSatPane::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (greylineModal_.isActive()) {
    return greylineModal_.onMouseUp(mx, my, mod, clicks);
  }
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onMouseUp(mx, my, mod, clicks);
  }

  // Modal: when menu is open, consume all clicks
  if (menuState_ != MenuState::Closed) {
    if (mx >= x_ && mx < x_ + width_ && my >= y_ && my < y_ + height_) {
      handleMenuClick(mx, my);
    } else {
      closeMenu();
    }
    return true;
  }

  // Hit test
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  if (mode_ == Mode::SAT) {
    if (mx >= trackButtonRect_.x &&
        mx <= trackButtonRect_.x + trackButtonRect_.w &&
        my >= trackButtonRect_.y &&
        my <= trackButtonRect_.y + trackButtonRect_.h) {
      if (satMgr_.getTrackedSatellite() == selectedSatName_) {
        satMgr_.trackSatellite("");
      } else {
        satMgr_.trackSatellite(selectedSatName_);
      }
      return true;
    }

    // Map Track button: toggle satellite ground track on world map
    if (mx >= mapTrackBtnRect_.x &&
        mx <= mapTrackBtnRect_.x + mapTrackBtnRect_.w &&
        my >= mapTrackBtnRect_.y &&
        my <= mapTrackBtnRect_.y + mapTrackBtnRect_.h) {
      mapTrackVisible_ = !mapTrackVisible_;
      if (onMapTrackToggle_)
        onMapTrackToggle_(mapTrackVisible_);
      return true;
    }
  }

  // 1. Forward to active panel first (to catch specific buttons like Greyline Sync)
  if (mode_ == Mode::DX) {
    if (dxPanel_.onMouseUp(mx, my, mod, clicks))
      return true;
  } else {
    if (satPanel_.onMouseUp(mx, my, mod, clicks))
      return true;
  }

  // 2. Upper 10% title bar — SAT mode opens satellite options menu;
  // DX mode returns false so PaneContainer handles widget selection.
  int headerH = std::max(1, height_ / 10);
  if (my < y_ + headerH) {
    if (mode_ == Mode::SAT) {
      openMenu();
      return true;
    }
    return false;
  }

  // 3. DX mode body click (lower 90%) not consumed by DXPanel — open satellite
  // list so the user can switch this pane to satellite mode.
  if (mode_ == Mode::DX) {
    openMenu();
    return true;
  }

  return false;
}

bool DXSatPane::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (greylineModal_.isActive()) {
    return true; // Modal consumes
  }
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onMouseDown(mx, my, mod, clicks);
  }
  return false;
}

bool DXSatPane::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (greylineModal_.isActive()) {
    return greylineModal_.onKeyDown(key, mod);
  }
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onKeyDown(key, mod);
  }

  if (menuState_ != MenuState::Closed) {
    if (key == SDLK_ESCAPE) {
      closeMenu();
      return true;
    }
    int maxVisible = (height_ - menuPad()) / menuItemHeight();
    int maxScroll =
        std::max(0, static_cast<int>(menuItems_.size()) - maxVisible);
    if (key == SDLK_UP && scrollOffset_ > 0) {
      --scrollOffset_;
      return true;
    }
    if (key == SDLK_DOWN && scrollOffset_ < maxScroll) {
      ++scrollOffset_;
      return true;
    }
    return true; // consume all keys while menu open
  }

  if (mode_ == Mode::DX)
    return dxPanel_.onKeyDown(key, mod);
  return satPanel_.onKeyDown(key, mod);
}

bool DXSatPane::onTextInput(const char *text) {
  if (greylineModal_.isActive()) {
    return true; // Modal consumes
  }
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onTextInput(text);
  }
  return false;
}

bool DXSatPane::onMouseWheel(int scrollY) {
  if (menuState_ == MenuState::Closed)
    return false;

  scrollOffset_ -= scrollY;
  if (scrollOffset_ < 0)
    scrollOffset_ = 0;
  int maxVisible = (height_ - menuPad()) / menuItemHeight();
  int maxScroll = std::max(0, static_cast<int>(menuItems_.size()) - maxVisible);
  scrollOffset_ = std::min(scrollOffset_, maxScroll);
  return true;
}

// --- Menu logic ---

int DXSatPane::menuPad() const { return static_cast<int>(width_ * 0.06f); }

int DXSatPane::menuItemHeight() const { return menuFontSize_ + menuPad(); }

void DXSatPane::openMenu() {
  if (mode_ == Mode::SAT) {
    menuState_ = MenuState::SatOptions;
  } else {
    menuState_ = MenuState::SatList;
  }
  scrollOffset_ = 0;
  populateMenu();
}

void DXSatPane::closeMenu() {
  menuState_ = MenuState::Closed;
  destroyMenuTextures();
  menuItems_.clear();
  satSnapshot_.clear();
}

void DXSatPane::populateMenu() {
  destroyMenuTextures();
  menuItems_.clear();

  if (menuState_ == MenuState::SatOptions) {
    menuItems_.push_back({"Choose satellites", kActionChooseSats, false});
    menuItems_.push_back(
        {"Add satellite...", 9999, false}); // Random high number for Add
    menuItems_.push_back({"Show DX Info here", kActionShowDX, false});
  } else if (menuState_ == MenuState::SatList) {
    satSnapshot_ = satMgr_.getSatellites();
    for (size_t i = 0; i < satSnapshot_.size(); ++i) {
      bool sel = (satSnapshot_[i].name == selectedSatName_);
      menuItems_.push_back({satSnapshot_[i].name, static_cast<int>(i), sel});
    }
    if (satSnapshot_.empty()) {
      menuItems_.push_back({"(Loading satellites...)", kActionNone, false});
    }
  }
}

void DXSatPane::destroyMenuTextures() {
  for (auto &item : menuItems_) {
    if (item.tex) {
      MemoryMonitor::getInstance().destroyTexture(item.tex);
    }
  }
}

void DXSatPane::handleMenuClick(int mx, int my) {
  (void)mx;
  int pad = menuPad();
  int itemH = menuItemHeight();
  int relY = my - y_ - pad;
  if (relY < 0) {
    closeMenu();
    return;
  }

  int idx = (relY / itemH) + scrollOffset_;
  if (idx < 0 || idx >= static_cast<int>(menuItems_.size())) {
    closeMenu();
    return;
  }

  executeAction(menuItems_[idx].action);
}

void DXSatPane::executeAction(int action) {
  if (action == kActionNone)
    return;

  if (action == kActionChooseSats) {
    menuState_ = MenuState::SatList;
    scrollOffset_ = 0;
    populateMenu();
    return;
  }

  if (action == kActionShowDX) {
    mode_ = Mode::DX;
    satPanel_.setPredictor(nullptr);
    closeMenu();
    notifyModeChanged();
    return;
  }

  if (action == 9999) {
    satelliteSetup_.setActive(true);
    closeMenu();
    return;
  }

  // Satellite index in satSnapshot_
  if (action >= 0 && action < static_cast<int>(satSnapshot_.size())) {
    auto &tle = satSnapshot_[action];
    if (predictor_.loadTLE(tle)) {
      selectedSatName_ = tle.name;
      satPanel_.setPredictor(&predictor_);
      mode_ = Mode::SAT;
    }
    closeMenu();
    notifyModeChanged();
  }
}

// --- Menu rendering ---

void DXSatPane::drawRadio(SDL_Renderer *renderer, int cx, int cy, int r,
                          bool filled) {
  ThemeColors themes = getThemeColors(theme_);
  // Outer circle
  SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 255);
  for (int dy = -r; dy <= r; ++dy) {
    int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }

  if (!filled) {
    // Punch out inner to create ring
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
    int inner = std::max(1, r - 2);
    for (int dy = -inner; dy <= inner; ++dy) {
      int dx = static_cast<int>(std::sqrt(inner * inner - dy * dy));
      SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
  }
}

void DXSatPane::renderMenu(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  // Background - force opaque (255) for all except glass to pop above dimming
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         (theme_ == "glass") ? 160 : 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  // Border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &bg);

  SDL_RenderSetClipRect(renderer, &bg);

  int pad = menuPad();
  int itemH = menuItemHeight();
  int radioR = std::max(3, menuFontSize_ / 3);
  int textX = x_ + pad + radioR * 2 + pad;
  int maxVisible = (height_ - pad) / itemH;

  SDL_Color white = themes.text;
  SDL_Color yellow = themes.accent;

  for (int vi = 0; vi < maxVisible &&
                   (scrollOffset_ + vi) < static_cast<int>(menuItems_.size());
       ++vi) {
    int idx = scrollOffset_ + vi;
    auto &item = menuItems_[idx];
    int curY = y_ + pad + vi * itemH;

    // Create texture on first render
    if (!item.tex) {
      SDL_Color color = item.selected ? yellow : white;
      item.tex = fontMgr_.renderText(renderer, item.label, color, menuFontSize_,
                                     &item.texW, &item.texH);
    }

    // Radio button
    int radioX = x_ + pad + radioR;
    int radioY = curY + itemH / 2;
    drawRadio(renderer, radioX, radioY, radioR, item.selected);

    // Label text
    if (item.tex) {
      SDL_Rect dst = {textX, curY + (itemH - item.texH) / 2, item.texW,
                      item.texH};
      SDL_RenderCopy(renderer, item.tex, nullptr, &dst);
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

std::string DXSatPane::getDisplayName() const {
  if (mode_ == Mode::SAT)
    return "Satellite";
  return "";
}

std::vector<std::string> DXSatPane::getActions() const {
  if (menuState_ == MenuState::Closed) {
    if (mode_ == Mode::SAT) {
      return {"open_menu", "track"};
    }
    return {"open_menu"};
  }
  return {};
}

SDL_Rect DXSatPane::getActionRect(const std::string &action) const {
  if (action == "open_menu") {
    // Upper 10%
    int headerH = std::max(1, height_ / 10);
    return {x_, y_, width_, headerH};
  }
  if (action == "track") {
    return trackButtonRect_;
  }
  return {0, 0, 0, 0};
}

nlohmann::json DXSatPane::getDebugData() const {
  if (menuState_ != MenuState::Closed) {
    return {{"menu_active", true}};
  }
  if (mode_ == Mode::DX) {
    return dxPanel_.getDebugData();
  }
  return satPanel_.getDebugData();
}
