#include "SatWidget.h"
#include "../core/Constants.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cmath>

SatWidget::SatWidget(int x, int y, int w, int h, FontManager &fontMgr,
                     TextureManager &texMgr, SatelliteManager &satMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr), satMgr_(satMgr),
      satPanel_(x, y, w, h, fontMgr, texMgr),
      satelliteSetup_(0, 0, HamClock::LOGICAL_WIDTH, HamClock::LOGICAL_HEIGHT,
                      fontMgr, satMgr) {}

SatWidget::~SatWidget() { destroyMenuTextures(); }

void SatWidget::setObserver(double latDeg, double lonDeg) {
  predictor_.setObserver(latDeg, lonDeg);
}

OrbitPredictor *SatWidget::activePredictor() {
  if (predictor_.isReady())
    return &predictor_;
  return nullptr;
}

void SatWidget::restoreState(const std::string &satName) {
  selectedSatName_ = satName;
  if (satName.empty())
    return;
  auto tle = satMgr_.findByName(satName);
  if (tle && predictor_.loadTLE(*tle)) {
    selectedSatName_ = tle->name;
    satPanel_.setPredictor(&predictor_);
  } else {
    pendingSatRestore_ = satName;
  }
}

void SatWidget::notifySatChanged() {
  if (onSatChanged_)
    onSatChanged_(selectedSatName_);
}

// --- Widget overrides ---

void SatWidget::update() {
  // Deferred satellite restore: retry once data arrives
  if (!pendingSatRestore_.empty() && satMgr_.hasData()) {
    auto tle = satMgr_.findByName(pendingSatRestore_);
    if (tle && predictor_.loadTLE(*tle)) {
      selectedSatName_ = tle->name;
      satPanel_.setPredictor(&predictor_);
    }
    pendingSatRestore_.clear();
  }

  if (menuOpen_ || satelliteSetup_.isActive())
    return;

  satPanel_.update();
}

void SatWidget::render(SDL_Renderer *renderer) {
  satPanel_.render(renderer);

  if (!isModalActive()) {
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

void SatWidget::renderModal(SDL_Renderer *renderer) {
  if (satelliteSetup_.isActive()) {
    satelliteSetup_.render(renderer);
  } else if (menuOpen_) {
    renderMenu(renderer);
  }
}

void SatWidget::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  satPanel_.onResize(x, y, w, h);
  satelliteSetup_.onResize(x, y, w, h);
  menuFontSize_ = fontMgr_.catalog()->ptSize(FontStyle::Fast);
  if (menuOpen_)
    destroyMenuTextures();
}

bool SatWidget::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onMouseUp(mx, my, mod, clicks);
  }

  // Modal: when menu is open, consume all clicks
  if (menuOpen_) {
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

  // Trk button
  if (mx >= trackButtonRect_.x && mx <= trackButtonRect_.x + trackButtonRect_.w &&
      my >= trackButtonRect_.y && my <= trackButtonRect_.y + trackButtonRect_.h) {
    if (satMgr_.getTrackedSatellite() == selectedSatName_) {
      satMgr_.trackSatellite("");
    } else {
      satMgr_.trackSatellite(selectedSatName_);
    }
    return true;
  }

  // Pth button
  if (mx >= mapTrackBtnRect_.x && mx <= mapTrackBtnRect_.x + mapTrackBtnRect_.w &&
      my >= mapTrackBtnRect_.y && my <= mapTrackBtnRect_.y + mapTrackBtnRect_.h) {
    mapTrackVisible_ = !mapTrackVisible_;
    if (onMapTrackToggle_)
      onMapTrackToggle_(mapTrackVisible_);
    return true;
  }

  // Forward to satPanel first (catches internal buttons)
  if (satPanel_.onMouseUp(mx, my, mod, clicks))
    return true;

  // Upper 10% header — open satellite list
  int headerH = std::max(1, height_ / 10);
  if (my < y_ + headerH) {
    openMenu();
    return true;
  }

  return false;
}

bool SatWidget::onMouseDown(int mx, int my, Uint16 mod, int clicks) {
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onMouseDown(mx, my, mod, clicks);
  }
  return false;
}

bool SatWidget::onKeyDown(SDL_Keycode key, Uint16 mod) {
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onKeyDown(key, mod);
  }

  if (menuOpen_) {
    if (key == SDLK_ESCAPE) {
      closeMenu();
      return true;
    }
    int maxVisible = (height_ - menuPad()) / menuItemHeight();
    int maxScroll = std::max(0, static_cast<int>(menuItems_.size()) - maxVisible);
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

  return satPanel_.onKeyDown(key, mod);
}

bool SatWidget::onTextInput(const char *text) {
  if (satelliteSetup_.isActive()) {
    return satelliteSetup_.onTextInput(text);
  }
  return false;
}

bool SatWidget::onMouseWheel(int scrollY) {
  if (!menuOpen_)
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

int SatWidget::menuPad() const { return static_cast<int>(width_ * 0.06f); }

int SatWidget::menuItemHeight() const { return menuFontSize_ + menuPad(); }

void SatWidget::openMenu() {
  menuOpen_ = true;
  scrollOffset_ = 0;
  populateMenu();
}

void SatWidget::closeMenu() {
  menuOpen_ = false;
  destroyMenuTextures();
  menuItems_.clear();
  satSnapshot_.clear();
}

void SatWidget::populateMenu() {
  destroyMenuTextures();
  menuItems_.clear();

  // "Add satellite..." is always first
  menuItems_.push_back({"Add satellite...", kActionAddSat, false});

  satSnapshot_ = satMgr_.getSatellites();
  for (size_t i = 0; i < satSnapshot_.size(); ++i) {
    bool sel = (satSnapshot_[i].name == selectedSatName_);
    menuItems_.push_back({satSnapshot_[i].name, static_cast<int>(i), sel});
  }
  if (satSnapshot_.empty()) {
    menuItems_.push_back({"(Loading satellites...)", kActionNone, false});
  }
}

void SatWidget::destroyMenuTextures() {
  for (auto &item : menuItems_) {
    if (item.tex) {
      MemoryMonitor::getInstance().destroyTexture(item.tex);
    }
  }
}

void SatWidget::handleMenuClick(int mx, int my) {
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

void SatWidget::executeAction(int action) {
  if (action == kActionNone)
    return;

  if (action == kActionAddSat) {
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
    }
    closeMenu();
    notifySatChanged();
  }
}

// --- Menu rendering ---

void SatWidget::drawRadio(SDL_Renderer *renderer, int cx, int cy, int r,
                          bool filled) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g, themes.text.b, 255);
  for (int dy = -r; dy <= r; ++dy) {
    int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }

  if (!filled) {
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
    int inner = std::max(1, r - 2);
    for (int dy = -inner; dy <= inner; ++dy) {
      int dx = static_cast<int>(std::sqrt(inner * inner - dy * dy));
      SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
  }
}

void SatWidget::renderMenu(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         (theme_ == "glass") ? 160 : 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

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

    if (!item.tex) {
      SDL_Color color = item.selected ? yellow : white;
      item.tex = fontMgr_.renderText(renderer, item.label, color, menuFontSize_,
                                     &item.texW, &item.texH);
    }

    int radioX = x_ + pad + radioR;
    int radioY = curY + itemH / 2;
    drawRadio(renderer, radioX, radioY, radioR, item.selected);

    if (item.tex) {
      SDL_Rect dst = {textX, curY + (itemH - item.texH) / 2, item.texW, item.texH};
      SDL_RenderCopy(renderer, item.tex, nullptr, &dst);
    }
  }

  SDL_RenderSetClipRect(renderer, nullptr);
}

std::vector<std::string> SatWidget::getActions() const {
  if (!menuOpen_)
    return {"open_menu", "track"};
  return {};
}

SDL_Rect SatWidget::getActionRect(const std::string &action) const {
  if (action == "open_menu") {
    int headerH = std::max(1, height_ / 10);
    return {x_, y_, width_, headerH};
  }
  if (action == "track")
    return trackButtonRect_;
  return {0, 0, 0, 0};
}
