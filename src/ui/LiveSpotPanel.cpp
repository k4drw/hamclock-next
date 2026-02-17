#include "LiveSpotPanel.h"
#include "../core/ConfigManager.h"
#include "../core/Constants.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"

#include <cstring>

LiveSpotPanel::LiveSpotPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             LiveSpotProvider &provider,
                             std::shared_ptr<LiveSpotDataStore> store,
                             AppConfig &config, ConfigManager &cfgMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr), provider_(provider),
      store_(std::move(store)), config_(config), cfgMgr_(cfgMgr) {
  // Initialize store with saved selection
  store_->setSelectedBandsMask(config_.liveSpotsBands);
}

void LiveSpotPanel::update() {
  uint32_t now = SDL_GetTicks();
  if (now - lastFetch_ > 5 * 60 * 1000 || lastFetch_ == 0) { // 5 mins
    lastFetch_ = now;
    provider_.fetch();
  }

  auto data = store_->snapshot();
  if (!data->valid)
    return;

  // Track selected bands (for visual highlight, no texture rebuild needed)
  std::memcpy(lastSelected_, data->selectedBands, sizeof(lastSelected_));

  // Check if counts changed
  bool changed = !dataValid_ || std::memcmp(data->bandCounts, lastCounts_,
                                            sizeof(lastCounts_)) != 0;
  if (changed) {
    std::memcpy(lastCounts_, data->bandCounts, sizeof(lastCounts_));
    dataValid_ = true;
    // Count textures will be rebuilt in render()
    for (auto &bc : bandCache_) {
      if (bc.countTex) {
        MemoryMonitor::getInstance().destroyTexture(bc.countTex);
      }
      bc.lastCount = -1;
    }

    // Dynamic subtitle based on source
    std::string srcStr = "PSK";
    if (config_.liveSpotSource == LiveSpotSource::RBN)
      srcStr = "RBN";
    if (config_.liveSpotSource == LiveSpotSource::WSPR)
      srcStr = "WSPR";

    std::string sub = "of " + data->grid + " - " + srcStr + " " +
                      std::to_string(config_.liveSpotsMaxAge) + " mins";
    if (sub != lastSubtitle_) {
      if (subtitleTex_) {
        MemoryMonitor::getInstance().destroyTexture(subtitleTex_);
        subtitleTex_ = nullptr;
      }
      lastSubtitle_ = sub;
    }
  }
}

void LiveSpotPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect bgRect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bgRect);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &bgRect);

  bool titleFontChanged = (titleFontSize_ != lastTitleFontSize_);
  bool cellFontChanged = (cellFontSize_ != lastCellFontSize_);

  SDL_Color white = themes.text;
  SDL_Color cyan = themes.accent;
  SDL_Color blue = themes.textDim;

  int pad = 2;
  int curY = y_ + pad;

  // --- Title: "Live Spots" (centered, blue) ---
  if (titleFontChanged || !titleTex_) {
    if (titleTex_) {
      MemoryMonitor::getInstance().destroyTexture(titleTex_);
    }
    titleTex_ = fontMgr_.renderText(renderer, "Live Spots", cyan,
                                    titleFontSize_, &titleW_, &titleH_);
    lastTitleFontSize_ = titleFontSize_;
  }
  if (titleTex_) {
    SDL_Rect dst = {x_ + (width_ - titleW_) / 2, curY, titleW_, titleH_};
    SDL_RenderCopy(renderer, titleTex_, nullptr, &dst);
    curY += titleH_ + 1;
  }

  // --- Subtitle: "of GRID - PSK 30 mins" (centered, blue) ---
  if (!lastSubtitle_.empty() && !subtitleTex_) {
    subtitleTex_ = fontMgr_.renderText(renderer, lastSubtitle_, blue,
                                       cellFontSize_, &subtitleW_, &subtitleH_);
  }
  if (subtitleTex_) {
    SDL_Rect dst = {x_ + (width_ - subtitleW_) / 2, curY, subtitleW_,
                    subtitleH_};
    SDL_RenderCopy(renderer, subtitleTex_, nullptr, &dst);
    curY += subtitleH_ + 1;
  }

  // --- Band count grid: 2 columns × 6 rows ---
  // Reserve space for footer
  int footerH = cellFontSize_ + 4;
  int gridBottom = y_ + height_ - footerH - pad;
  int gridH = gridBottom - curY;
  if (gridH < 10)
    return;

  int rows = kNumBands / 2;
  int cellH = gridH / rows;
  int colW = (width_ - 2 * pad) / 2;
  int gap = 1; // pixel gap between cells

  // Cache grid geometry for onMouseUp hit-testing
  gridTop_ = curY;
  gridBottom_ = gridBottom;
  gridCellH_ = cellH;
  gridColW_ = colW;
  gridPad_ = pad;

  if (cellFontChanged) {
    // Rebuild all band label textures
    for (int i = 0; i < kNumBands; ++i) {
      if (bandCache_[i].labelTex) {
        MemoryMonitor::getInstance().destroyTexture(bandCache_[i].labelTex);
      }
      if (bandCache_[i].countTex) {
        MemoryMonitor::getInstance().destroyTexture(bandCache_[i].countTex);
      }
      bandCache_[i].lastCount = -1;
    }
    lastCellFontSize_ = cellFontSize_;
  }

  for (int i = 0; i < kNumBands; ++i) {
    int col = i / rows; // 0 = left, 1 = right
    int row = i % rows;
    int cx = x_ + pad + col * colW;
    int cy = curY + row * cellH;

    // Background: colored only if selected for display (map plotting)
    const auto &bd = kBands[i];
    if (lastSelected_[i]) {
      SDL_SetRenderDrawColor(renderer, bd.color.r, bd.color.g, bd.color.b, 255);
    } else {
      // Dark background for unselected bands
      SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
    }
    SDL_Rect cellRect = {cx + gap, cy + gap, colW - 2 * gap, cellH - 2 * gap};
    SDL_RenderFillRect(renderer, &cellRect);

    // Band label (left-aligned, cached)
    if (!bandCache_[i].labelTex) {
      bandCache_[i].labelTex =
          fontMgr_.renderText(renderer, bd.name, white, cellFontSize_,
                              &bandCache_[i].labelW, &bandCache_[i].labelH);
    }
    if (bandCache_[i].labelTex) {
      int ty = cy + gap + (cellH - 2 * gap - bandCache_[i].labelH) / 2;
      SDL_Rect dst = {cx + gap + 2, ty, bandCache_[i].labelW,
                      bandCache_[i].labelH};
      SDL_RenderCopy(renderer, bandCache_[i].labelTex, nullptr, &dst);
    }

    // Count (right-aligned, cached, rebuilt when value changes)
    int count = lastCounts_[i];
    if (bandCache_[i].lastCount != count) {
      if (bandCache_[i].countTex) {
        MemoryMonitor::getInstance().destroyTexture(bandCache_[i].countTex);
      }
      bandCache_[i].countTex = fontMgr_.renderText(
          renderer, std::to_string(count), white, cellFontSize_,
          &bandCache_[i].countW, &bandCache_[i].countH);
      bandCache_[i].lastCount = count;
    }
    if (bandCache_[i].countTex) {
      int ty = cy + gap + (cellH - 2 * gap - bandCache_[i].countH) / 2;
      int tx = cx + colW - gap - 2 - bandCache_[i].countW;
      SDL_Rect dst = {tx, ty, bandCache_[i].countW, bandCache_[i].countH};
      SDL_RenderCopy(renderer, bandCache_[i].countTex, nullptr, &dst);
    }
  }

  // --- Footer: "Counts" (centered, white) ---
  if (!footerTex_ || cellFontChanged) {
    if (footerTex_) {
      MemoryMonitor::getInstance().destroyTexture(footerTex_);
    }
    footerTex_ = fontMgr_.renderText(renderer, "Counts", white, cellFontSize_,
                                     &footerW_, &footerH_);
  }
  if (footerTex_) {
    int fy = gridBottom + (footerH - footerH_) / 2;
    footerRect_ = {x_ + (width_ - footerW_) / 2, fy, footerW_, footerH_};
    SDL_RenderCopy(renderer, footerTex_, nullptr, &footerRect_);
  }
}

void LiveSpotPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);

  // --- Pop-out Setup Menu Centered on screen ---
  int menuW = 320;
  int menuH = 260;
  int menuX = (HamClock::LOGICAL_WIDTH - menuW) / 2;
  int menuY = (HamClock::LOGICAL_HEIGHT - menuH) / 2;
  menuRect_ = {menuX, menuY, menuW, menuH};

  // Background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &menuRect_);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &menuRect_);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;
  SDL_Color blue = themes.textDim;
  SDL_Color green = themes.success;

  int y = menuY + 10;
  int cx = menuX + menuW / 2;

  // --- Tabs: PSK | RBN | WSPR ---
  const char *tabs[] = {"PSK", "RBN", "WSPR"};
  int tabCount = 3;
  int tabW = (menuW - 24) / tabCount;
  int tabH = 24;
  for (int i = 0; i < tabCount; ++i) {
    tabRects_[i] = {menuX + 12 + i * tabW, y, tabW, tabH};
    bool active = (activeTab_ == static_cast<LiveSpotSource>(i));
    SDL_SetRenderDrawColor(renderer, active ? 60 : 30, active ? 60 : 30,
                           active ? 80 : 40, 255);
    SDL_RenderFillRect(renderer, &tabRects_[i]);
    SDL_SetRenderDrawColor(renderer, active ? 100 : 60, active ? 100 : 60,
                           active ? 150 : 80, 255);
    SDL_RenderDrawRect(renderer, &tabRects_[i]);

    int tw, th;
    SDL_Texture *t = fontMgr_.renderText(
        renderer, tabs[i], active ? white : blue, cellFontSize_ + 2, &tw, &th);
    if (t) {
      SDL_Rect tr = {tabRects_[i].x + (tabW - tw) / 2,
                     tabRects_[i].y + (tabH - th) / 2, tw, th};
      SDL_RenderCopy(renderer, t, nullptr, &tr);
      MemoryMonitor::getInstance().destroyTexture(t);
    }
  }
  y += tabH + 16;

  int lx = menuX + 16;
  int tw, th;

  // Mode: DE/DX
  SDL_Rect box = {lx, y, 16, 16};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &box);
  if (pendingOfDe_) {
    SDL_SetRenderDrawColor(renderer, green.r, green.g, green.b, 255);
    SDL_Rect inner = {lx + 3, y + 3, 10, 10};
    SDL_RenderFillRect(renderer, &inner);
  }
  modeCheckRect_ = {lx, y, menuW - 32, 16};

  std::string modeTxt =
      pendingOfDe_ ? "Mode: DE (Spots OF Me)" : "Mode: DX (Spots BY Me)";
  SDL_Texture *t = fontMgr_.renderText(renderer, modeTxt, white,
                                       cellFontSize_ + 2, &tw, &th);
  if (t) {
    SDL_Rect tr = {lx + 24, y + (16 - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  y += 28;

  // Filter: Call/Grid
  box = {lx, y, 16, 16};
  SDL_SetRenderDrawColor(renderer, 50, 50, 60, 255);
  SDL_RenderFillRect(renderer, &box);
  SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
  SDL_RenderDrawRect(renderer, &box);
  if (pendingUseCall_) {
    SDL_SetRenderDrawColor(renderer, green.r, green.g, green.b, 255);
    SDL_Rect inner = {lx + 3, y + 3, 10, 10};
    SDL_RenderFillRect(renderer, &inner);
  }
  filterCheckRect_ = {lx, y, menuW - 32, 16};

  std::string filterTxt =
      pendingUseCall_
          ? "Filter: Callsign (" + config_.callsign + ")"
          : "Filter: Grid (" +
                (config_.grid.size() >= 4 ? config_.grid.substr(0, 4)
                                          : config_.grid) +
                ")";
  t = fontMgr_.renderText(renderer, filterTxt, white, cellFontSize_ + 2, &tw,
                          &th);
  if (t) {
    SDL_Rect tr = {lx + 24, y + (16 - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  y += 28;

  // Max Age
  t = fontMgr_.renderText(renderer, "Max Age (mins):", blue, cellFontSize_ + 2,
                          &tw, &th);
  if (t) {
    SDL_Rect tr = {lx, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  int ageX = lx + 120;
  ageDecrRect_ = {ageX, y - 2, 24, 24};
  ageIncrRect_ = {ageX + 60, y - 2, 24, 24};

  SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
  SDL_RenderFillRect(renderer, &ageDecrRect_);
  SDL_RenderFillRect(renderer, &ageIncrRect_);
  SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
  SDL_RenderDrawRect(renderer, &ageDecrRect_);
  SDL_RenderDrawRect(renderer, &ageIncrRect_);

  t = fontMgr_.renderText(renderer, "-", white, cellFontSize_ + 4, &tw, &th);
  if (t) {
    SDL_Rect tr = {ageDecrRect_.x + (24 - tw) / 2,
                   ageDecrRect_.y + (24 - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  t = fontMgr_.renderText(renderer, "+", white, cellFontSize_ + 4, &tw, &th);
  if (t) {
    SDL_Rect tr = {ageIncrRect_.x + (24 - tw) / 2,
                   ageIncrRect_.y + (24 - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  t = fontMgr_.renderText(renderer, std::to_string(pendingMaxAge_), white,
                          cellFontSize_ + 4, &tw, &th);
  if (t) {
    SDL_Rect tr = {ageX + 32, y, 20, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  y += 36;

  // Info
  std::string info;
  if (activeTab_ == LiveSpotSource::PSK)
    info = "Fetch via PSKReporter XML API";
  else if (activeTab_ == LiveSpotSource::RBN)
    info = "Real-time telnet feed (Telnet RBN)";
  else
    info = "WSPRnet streaming (experimental)";

  t = fontMgr_.renderText(renderer, info, blue, 10, &tw, &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  // Buttons at bottom
  int btnW = 80;
  int btnH = 32;
  int btnY = menuY + menuH - btnH - 12;

  cancelBtnRect_ = {cx - btnW - 10, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 60, 20, 20, 255);
  SDL_RenderFillRect(renderer, &cancelBtnRect_);
  SDL_SetRenderDrawColor(renderer, 150, 50, 50, 255);
  SDL_RenderDrawRect(renderer, &cancelBtnRect_);
  t = fontMgr_.renderText(renderer, "Cancel", white, cellFontSize_ + 2, &tw,
                          &th);
  if (t) {
    SDL_Rect tr = {cancelBtnRect_.x + (btnW - tw) / 2,
                   cancelBtnRect_.y + (btnH - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }

  doneBtnRect_ = {cx + 10, btnY, btnW, btnH};
  SDL_SetRenderDrawColor(renderer, 20, 60, 20, 255);
  SDL_RenderFillRect(renderer, &doneBtnRect_);
  SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
  SDL_RenderDrawRect(renderer, &doneBtnRect_);
  t = fontMgr_.renderText(renderer, "Done", white, cellFontSize_ + 2, &tw, &th);
  if (t) {
    SDL_Rect tr = {doneBtnRect_.x + (btnW - tw) / 2,
                   doneBtnRect_.y + (btnH - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
}

bool LiveSpotPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (showSetup_) {
    // Modal setup consumes all clicks. If outside menu, close as cancel?
    // Usually HamClock clicks outside dismiss.
    if (mx < menuRect_.x || mx >= menuRect_.x + menuRect_.w ||
        my < menuRect_.y || my >= menuRect_.y + menuRect_.h) {
      showSetup_ = false;
      return true;
    }
    return handleSetupClick(mx, my);
  }

  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  // Check footer click -> Open setup
  if (mx >= footerRect_.x && mx <= footerRect_.x + footerRect_.w &&
      my >= footerRect_.y && my <= footerRect_.y + footerRect_.h) {
    showSetup_ = true;
    activeTab_ = config_.liveSpotSource;
    pendingOfDe_ = config_.liveSpotsOfDe;
    pendingUseCall_ = config_.liveSpotsUseCall;
    pendingMaxAge_ = config_.liveSpotsMaxAge;
    return true;
  }

  // Use cached grid geometry from last render() call
  if (gridCellH_ <= 0 || gridColW_ <= 0)
    return false;

  int rows = kNumBands / 2;

  int relX = mx - (x_ + gridPad_);
  int relY = my - gridTop_;
  if (relX < 0 || relY < 0)
    return false;

  int col = relX / gridColW_;
  int row = relY / gridCellH_;
  if (col < 0 || col > 1 || row < 0 || row >= rows)
    return false;

  int bandIdx = col * rows + row;
  if (bandIdx < 0 || bandIdx >= kNumBands)
    return false;

  store_->toggleBand(bandIdx);
  // Persist the change
  config_.liveSpotsBands = store_->getSelectedBandsMask();
  cfgMgr_.save(config_);
  return true;
}

bool LiveSpotPanel::handleSetupClick(int mx, int my) {
  // Tabs
  for (int i = 0; i < 3; ++i) {
    if (mx >= tabRects_[i].x && mx <= tabRects_[i].x + tabRects_[i].w &&
        my >= tabRects_[i].y && my <= tabRects_[i].y + tabRects_[i].h) {
      activeTab_ = static_cast<LiveSpotSource>(i);
      return true;
    }
  }

  // Mode/Filter Checkboxes
  if (mx >= modeCheckRect_.x && mx <= modeCheckRect_.x + modeCheckRect_.w &&
      my >= modeCheckRect_.y && my <= modeCheckRect_.y + modeCheckRect_.h) {
    pendingOfDe_ = !pendingOfDe_;
    return true;
  }
  if (mx >= filterCheckRect_.x &&
      mx <= filterCheckRect_.x + filterCheckRect_.w &&
      my >= filterCheckRect_.y &&
      my <= filterCheckRect_.y + filterCheckRect_.h) {
    pendingUseCall_ = !pendingUseCall_;
    return true;
  }

  // Age Buttons
  if (mx >= ageDecrRect_.x && mx <= ageDecrRect_.x + ageDecrRect_.w &&
      my >= ageDecrRect_.y && my <= ageDecrRect_.y + ageDecrRect_.h) {
    if (pendingMaxAge_ > 15)
      pendingMaxAge_ -= 15;
    return true;
  }
  if (mx >= ageIncrRect_.x && mx <= ageIncrRect_.x + ageIncrRect_.w &&
      my >= ageIncrRect_.y && my <= ageIncrRect_.y + ageIncrRect_.h) {
    if (pendingMaxAge_ < 1440)
      pendingMaxAge_ += 15;
    return true;
  }

  // Pop-out Action Buttons
  if (mx >= cancelBtnRect_.x && mx <= cancelBtnRect_.x + cancelBtnRect_.w &&
      my >= cancelBtnRect_.y && my <= cancelBtnRect_.y + cancelBtnRect_.h) {
    showSetup_ = false;
    return true;
  }
  if (mx >= doneBtnRect_.x && mx <= doneBtnRect_.x + doneBtnRect_.w &&
      my >= doneBtnRect_.y && my <= doneBtnRect_.y + doneBtnRect_.h) {
    // Save
    config_.liveSpotSource = activeTab_;
    config_.liveSpotsOfDe = pendingOfDe_;
    config_.liveSpotsUseCall = pendingUseCall_;
    config_.liveSpotsMaxAge = pendingMaxAge_;
    cfgMgr_.save(config_);

    // Clear and refetch if source changed
    store_->clearSpots();
    provider_.updateConfig(config_);
    provider_.fetch(); // Force refresh with new settings

    showSetup_ = false;
    return true;
  }
  return true;
}

void LiveSpotPanel::destroyCache() {
  if (titleTex_) {
    MemoryMonitor::getInstance().destroyTexture(titleTex_);
  }
  if (subtitleTex_) {
    MemoryMonitor::getInstance().destroyTexture(subtitleTex_);
  }
  if (footerTex_) {
    MemoryMonitor::getInstance().destroyTexture(footerTex_);
  }
  for (auto &bc : bandCache_) {
    MemoryMonitor::getInstance().destroyTexture(bc.labelTex);
    MemoryMonitor::getInstance().destroyTexture(bc.countTex);
    bc.lastCount = -1;
  }
  lastTitleFontSize_ = 0;
  lastCellFontSize_ = 0;
  lastSubtitle_.clear();
}

void LiveSpotPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  destroyCache();
}

std::vector<std::string> LiveSpotPanel::getActions() const {
  std::vector<std::string> actions;
  for (int i = 0; i < kNumBands; ++i) {
    actions.push_back("toggle_band_" + std::to_string(i));
  }
  return actions;
}

SDL_Rect LiveSpotPanel::getActionRect(const std::string &action) const {
  // Expected format: toggle_band_N
  if (action.find("toggle_band_") == 0) {
    try {
      int idx = std::stoi(action.substr(12));
      if (idx >= 0 && idx < kNumBands) {
        // Re-calculate cell position based on verified grid logic from render()
        // Grid is 2 columns x (kNumBands/2) rows
        int rows = kNumBands / 2;
        int col = idx / rows;
        int row = idx % rows;

        // We need to know gridTop_ etc, which are cached in render()
        // If render hasn't run, these might be 0.
        int pad = 2; // Hardcoded pad from render
        int colW = (width_ - 2 * pad) / 2;
        // height of grid area roughly
        int footerH = 14; // approx
        int gH = (height_ - footerH - pad) - gridTop_;
        if (gH > 0) {
          int cellH = gH / rows;
          int cx = x_ + pad + col * colW;
          int cy = gridTop_ + row * cellH;
          return {cx, cy, colW, cellH};
        }
      }
    } catch (...) {
    }
  }
  return {0, 0, 0, 0};
}

bool LiveSpotPanel::performAction(const std::string &action) {
  if (action.find("toggle_band_") == 0) {
    try {
      int idx = std::stoi(action.substr(12));
      if (idx >= 0 && idx < kNumBands) {
        store_->toggleBand(idx);
        config_.liveSpotsBands = store_->getSelectedBandsMask();
        cfgMgr_.save(config_);
        return true;
      }
    } catch (...) {
    }
  }
  return false;
}

nlohmann::json LiveSpotPanel::getDebugData() const {
  nlohmann::json j;
  auto data = store_->snapshot();
  j["grid"] = data->grid;
  j["windowMinutes"] = data->windowMinutes;
  j["selectedBands"] = config_.liveSpotsBands;
  return j;
}
