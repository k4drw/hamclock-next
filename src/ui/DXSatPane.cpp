#include "DXSatPane.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cmath>

DXSatPane::DXSatPane(int x, int y, int w, int h, FontManager &fontMgr,
                     std::shared_ptr<HamClockState> state,
                     SatelliteManager &satMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), state_(state), satMgr_(satMgr),
      dxPanel_(x, y, w, h, fontMgr, state), satPanel_(x, y, w, h, fontMgr) {}

DXSatPane::~DXSatPane() { destroyMenuTextures(); }

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
    const SatelliteTLE *tle = satMgr_.findByName(satName);
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
    const SatelliteTLE *tle = satMgr_.findByName(pendingSatRestore_);
    if (tle && predictor_.loadTLE(*tle)) {
      selectedSatName_ = tle->name;
      satPanel_.setPredictor(&predictor_);
      mode_ = Mode::SAT;
    }
    pendingSatRestore_.clear();
  }

  if (menuState_ != MenuState::Closed)
    return;

  if (mode_ == Mode::DX) {
    dxPanel_.update();
  } else {
    satPanel_.update();
  }
}

void DXSatPane::render(SDL_Renderer *renderer) {
  if (menuState_ != MenuState::Closed) {
    renderMenu(renderer);
  } else if (mode_ == Mode::DX) {
    dxPanel_.render(renderer);
  } else {
    satPanel_.render(renderer);
  }
}

void DXSatPane::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  dxPanel_.onResize(x, y, w, h);
  satPanel_.onResize(x, y, w, h);
  menuFontSize_ = fontMgr_.catalog()->ptSize(FontStyle::Fast);
  if (menuState_ != MenuState::Closed)
    destroyMenuTextures();
}

bool DXSatPane::onMouseUp(int mx, int my, Uint16 mod) {
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

  // Upper 10% → open menu
  int headerH = std::max(1, height_ / 10);
  if (my < y_ + headerH) {
    openMenu();
    return true;
  }

  // Forward to active panel
  if (mode_ == Mode::DX)
    return dxPanel_.onMouseUp(mx, my, mod);
  return satPanel_.onMouseUp(mx, my, mod);
}

bool DXSatPane::onKeyDown(SDL_Keycode key, Uint16 mod) {
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
      SDL_DestroyTexture(item.tex);
      item.tex = nullptr;
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
  // White outer circle
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  for (int dy = -r; dy <= r; ++dy) {
    int dx = static_cast<int>(std::sqrt(r * r - dy * dy));
    SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
  }

  if (!filled) {
    // Punch out inner to create ring
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    int inner = std::max(1, r - 2);
    for (int dy = -inner; dy <= inner; ++dy) {
      int dx = static_cast<int>(std::sqrt(inner * inner - dy * dy));
      SDL_RenderDrawLine(renderer, cx - dx, cy + dy, cx + dx, cy + dy);
    }
  }
}

void DXSatPane::renderMenu(SDL_Renderer *renderer) {
  // Dark background
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  // Border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_RenderDrawRect(renderer, &bg);

  SDL_RenderSetClipRect(renderer, &bg);

  int pad = menuPad();
  int itemH = menuItemHeight();
  int radioR = std::max(3, menuFontSize_ / 3);
  int textX = x_ + pad + radioR * 2 + pad;
  int maxVisible = (height_ - pad) / itemH;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color yellow = {255, 255, 0, 255};

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
