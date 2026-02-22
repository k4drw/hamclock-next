#include "ActivityPanels.h"
#include "../core/LiveSpotData.h" // For kBands and freqToBandIndex
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "RenderUtils.h" // ADDED
#include <algorithm>
#include <iomanip>
#include <sstream>

// --- DXPedPanel ---

DXPedPanel::DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       ActivityProvider &provider,
                       std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Peditions", {}), provider_(provider),
      store_(store) {}

void DXPedPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 20 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    std::vector<std::string> rows;
    for (const auto &de : data.dxpeds) {
      std::stringstream ss;
      ss << std::left << std::setw(12) << de.call << de.location;
      rows.push_back(ss.str());
      if (rows.size() >= 10)
        break;
    }
    if (rows.empty() && data.valid) {
      rows.push_back("No upcoming expeditions");
    }
    setRows(rows);
    lastUpdate_ = data.lastUpdated;
  }
}

// --- ONTAPanel ---

ONTAPanel::ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     ActivityProvider &provider,
                     std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "On The Air", {}), provider_(provider),
      store_(store) {}

ONTAPanel::~ONTAPanel() { clearSpotCache(); }

void ONTAPanel::clearSpotCache() {
  for (auto &cs : spotCache_) {
    if (cs.modeTex)
      MemoryMonitor::getInstance().destroyTexture(cs.modeTex);
    if (cs.callTex)
      MemoryMonitor::getInstance().destroyTexture(cs.callTex);
    if (cs.refTex)
      MemoryMonitor::getInstance().destroyTexture(cs.refTex);
    if (cs.progTex)
      MemoryMonitor::getInstance().destroyTexture(cs.progTex);
  }
  spotCache_.clear();
  if (footerTex_) {
    MemoryMonitor::getInstance().destroyTexture(footerTex_);
    footerTex_ = nullptr;
  }
}

const char *ONTAPanel::filterLabel(Filter f) {
  switch (f) {
  case Filter::POTA:
    return "POTA";
  case Filter::SOTA:
    return "SOTA";
  default:
    return "ALL";
  }
}

void ONTAPanel::setFilter(const std::string &f) {
  if (f == "pota")
    filter_ = Filter::POTA;
  else if (f == "sota")
    filter_ = Filter::SOTA;
  else
    filter_ = Filter::ALL;
  // Force row rebuild on next update
  lastUpdate_ = {};
}

void ONTAPanel::rebuildRows(const ActivityData &data) {
  currentSpots_.clear();
  std::vector<std::string> rows; // Use this to pass empty strings to ListPanel

  for (const auto &os : data.ontaSpots) {
    if (filter_ == Filter::POTA && os.program != "POTA")
      continue;
    if (filter_ == Filter::SOTA && os.program != "SOTA")
      continue;

    currentSpots_.push_back(os);
    rows.push_back(""); // Add empty string for ListPanel to draw stripes
    if (currentSpots_.size() >= MAX_VISIBLE_ROWS)
      break;
  }
  if (currentSpots_.empty() && data.valid) {
    std::string prog = (filter_ == Filter::POTA)   ? "POTA"
                       : (filter_ == Filter::SOTA) ? "SOTA"
                                                   : "";
    rows.push_back("No active" + (prog.empty() ? "" : " " + prog) + " spots");
  }
  setRows(rows);
}

void ONTAPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 5 * 60 * 1000 || lastFetch_ == 0) {
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    rebuildRows(data);
    lastUpdate_ = data.lastUpdated;
  }

  // Update highlight from selection
  if (data.hasSelection) {
    int foundIdx = -1;
    for (size_t i = 0; i < currentSpots_.size(); ++i) {
      if (currentSpots_[i].call == data.selectedSpot.call &&
          currentSpots_[i].ref == data.selectedSpot.ref) {
        foundIdx = static_cast<int>(i);
        break;
      }
    }
    setHighlightedIndex(foundIdx);
  } else {
    setHighlightedIndex(-1);
  }

  // Resize spot cache
  if (spotCache_.size() != currentSpots_.size()) {
    clearSpotCache();
    spotCache_.resize(currentSpots_.size());
  }
}

void ONTAPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  clearSpotCache();
}

SDL_Color ONTAPanel::getRowColor(int index,
                                 const SDL_Color &defaultColor) const {
  if (index >= 0 && index < (int)currentSpots_.size()) {
    int bandIdx = freqToBandIndex(currentSpots_[index].freqKhz);
    if (bandIdx >= 0) {
      return kBands[bandIdx].color;
    }
  }
  return defaultColor;
}

void ONTAPanel::render(SDL_Renderer *renderer) {
  // Let ListPanel draw background, border, title, and rows (empty strings)
  ListPanel::render(renderer);

  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int titleAreaH = pad * 2;
  if (titleTex_) {
    titleAreaH += titleH_;
  }

  std::string chip = filterLabel(filter_);
  int chipFontSize = rowFontSize_;
  int cw = 0, ch = 0;
  TTF_Font *font = fontMgr_.getFont(chipFontSize);
  if (font) {
    TTF_SizeText(font, chip.c_str(), &cw, &ch);
  }

  // Visual button background centered vertically in title area
  int btnH = 20;
  int btnY = y_ + (titleAreaH - btnH) / 2;
  int chipX = x_ + width_ - pad - cw - 5;
  SDL_Rect btnRect = {chipX - 5, btnY, cw + 10, btnH};
  SDL_Color chipColor = (filter_ != Filter::ALL) ? themes.accent : themes.info;

  // Render concept: Semi-transparent black background + border matching text
  // color
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
  SDL_RenderFillRect(renderer, &btnRect);

  SDL_SetRenderDrawColor(renderer, chipColor.r, chipColor.g, chipColor.b, 255);
  SDL_RenderDrawRect(renderer, &btnRect);

  fontMgr_.drawText(renderer, chip, btnRect.x + btnRect.w / 2,
                    btnRect.y + btnRect.h / 2, chipColor, chipFontSize, false,
                    true);

  // Generous hit box spans full title height
  chipRect_ = {btnRect.x, y_, btnRect.w, titleAreaH};

  if (currentSpots_.empty()) {
    return;
  }

  int curY = y_ + titleAreaH;

  // Calculate row height
  int remaining = (y_ + height_) - curY;
  int rowCount = static_cast<int>(currentSpots_.size());
  int rowH = std::max(rowFontSize_ + 4, remaining / rowCount);

  // Column layout: Mode (Left) | Call (Left) | Ref (Left) | Program (Right)
  int modeX = x_ + pad;
  int callX = modeX + fontMgr_.getLogicalWidth("CW", rowFontSize_) + 10;
  int refX = callX + fontMgr_.getLogicalWidth("W7UUU", rowFontSize_) + 10;
  int progXEnd = x_ + width_ - pad;

  for (size_t i = 0; i < currentSpots_.size(); ++i) {
    if (i >= spotCache_.size())
      break;

    int rowY = curY + static_cast<int>(i) * rowH;
    const auto &spot = currentSpots_[i];
    auto &cache = spotCache_[i];
    SDL_Color color = getRowColor(static_cast<int>(i), {255, 255, 255, 255});

    // 1. Mode
    if (!cache.modeTex || cache.lastMode != spot.mode) {
      if (cache.modeTex)
        MemoryMonitor::getInstance().destroyTexture(cache.modeTex);
      cache.modeTex = fontMgr_.renderText(
          renderer, spot.mode, color, rowFontSize_, &cache.modeW, &cache.modeH);
      cache.lastMode = spot.mode;
    }
    if (cache.modeTex) {
      int ty = rowY + (rowH - cache.modeH) / 2;
      SDL_Rect dst = {modeX, ty, cache.modeW, cache.modeH};
      SDL_RenderCopy(renderer, cache.modeTex, nullptr, &dst);
    }

    // 2. Call
    if (!cache.callTex || cache.lastCall != spot.call) {
      if (cache.callTex)
        MemoryMonitor::getInstance().destroyTexture(cache.callTex);
      cache.callTex = fontMgr_.renderText(
          renderer, spot.call, color, rowFontSize_, &cache.callW, &cache.callH);
      cache.lastCall = spot.call;
    }
    if (cache.callTex) {
      int ty = rowY + (rowH - cache.callH) / 2;
      SDL_Rect dst = {callX, ty, cache.callW, cache.callH};
      SDL_RenderCopy(renderer, cache.callTex, nullptr, &dst);
    }

    // 3. Ref
    if (!cache.refTex || cache.lastRef != spot.ref) {
      if (cache.refTex)
        MemoryMonitor::getInstance().destroyTexture(cache.refTex);
      cache.refTex = fontMgr_.renderText(
          renderer, spot.ref, color, rowFontSize_, &cache.refW, &cache.refH);
      cache.lastRef = spot.ref;
    }
    if (cache.refTex) {
      int ty = rowY + (rowH - cache.refH) / 2;
      SDL_Rect dst = {refX, ty, cache.refW, cache.refH};
      SDL_RenderCopy(renderer, cache.refTex, nullptr, &dst);
    }

    // 4. Program (POTA/SOTA -> P/S)
    std::string shortProg =
        spot.program.empty() ? "" : spot.program.substr(0, 1);
    if (!cache.progTex || cache.lastProg != shortProg) {
      if (cache.progTex)
        MemoryMonitor::getInstance().destroyTexture(cache.progTex);
      cache.progTex = fontMgr_.renderText(
          renderer, shortProg, color, rowFontSize_, &cache.progW, &cache.progH);
      cache.lastProg = shortProg;
    }
    if (cache.progTex) {
      int ty = rowY + (rowH - cache.progH) / 2;
      SDL_Rect dst = {progXEnd - cache.progW, ty, cache.progW, cache.progH};
      SDL_RenderCopy(renderer, cache.progTex, nullptr, &dst);
    }
  }
}

bool ONTAPanel::onMouseUp(int mx, int my, Uint16 mod) {
  (void)mod;

  if (showSetup_) {
    return handleSetupClick(mx, my);
  }

  // 1. Check chip hit FIRST (it might be right at the top edge)
  if (chipRect_.w > 0 && mx >= chipRect_.x && mx < chipRect_.x + chipRect_.w &&
      my >= chipRect_.y && my < chipRect_.y + chipRect_.h) {
    if (filter_ == Filter::ALL)
      filter_ = Filter::POTA;
    else if (filter_ == Filter::POTA)
      filter_ = Filter::SOTA;
    else
      filter_ = Filter::ALL;

    // Force row rebuild on next update
    lastUpdate_ = {};

    if (onFilterChanged_) {
      std::string fstr;
      switch (filter_) {
      case Filter::POTA:
        fstr = "pota";
        break;
      case Filter::SOTA:
        fstr = "sota";
        break;
      default:
        fstr = "all";
        break;
      }
      onFilterChanged_(fstr);
    }
    return true;
  }

  // Bounds check for the rest of the panel
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  // 2. Check title hit (left of chip) to open setup modal
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int titleAreaH = pad * 2;
  if (titleTex_)
    titleAreaH += titleH_;
  if (my >= y_ && my <= y_ + titleAreaH) {
    showSetup_ = true;
    pendingFilter_ = filter_;
    return true;
  }

  if (my > y_ + titleAreaH) {
    int rowY = my - (y_ + titleAreaH);
    int rowH =
        rowFontSize_ + pad; // rowFontSize_ is ListPanel's protected member
    if (rowH > 0) {
      size_t idx = rowY / rowH;
      if (idx < currentSpots_.size()) {
        auto data = store_->get();
        data.hasSelection = true;
        data.selectedSpot = currentSpots_[idx];
        store_->set(data);
        setHighlightedIndex(static_cast<int>(idx));
        return true;
      }
    }
  }

  return false;
}

bool ONTAPanel::isModalActive() const { return showSetup_; }

void ONTAPanel::renderModal(SDL_Renderer *renderer) { renderSetup(renderer); }

void ONTAPanel::renderSetup(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  // Background
  SDL_Rect bg = {x_, y_, width_, height_};
  RenderUtils::drawRect(renderer, (float)bg.x, (float)bg.y, (float)bg.w,
                        (float)bg.h, themes.bg);
  RenderUtils::drawRect(renderer, (float)bg.x, (float)bg.y, (float)bg.w,
                        (float)bg.h, themes.border);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;

  int y = y_ + 10;
  int cx = x_ + width_ / 2;

  // Title
  int tw, th;
  SDL_Texture *t = fontMgr_.renderText(renderer, "--- ONTA Filter ---", cyan,
                                       titleFontSize_, &tw, &th);
  if (t) {
    SDL_Rect tr = {cx - tw / 2, y, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
  y += th + 15;
  int lx = x_ + 10;
  int btnW = width_ - 20;

  auto renderButton = [&](const char *label, Filter f, SDL_Rect &rect) {
    bool selected = (pendingFilter_ == f);
    rect = {lx, y, btnW, 22};
    RenderUtils::drawRect(renderer, (float)rect.x, (float)rect.y, (float)rect.w,
                          (float)rect.h,
                          selected ? themes.accent : themes.rowStripe1);
    RenderUtils::drawRect(renderer, (float)rect.x, (float)rect.y, (float)rect.w,
                          (float)rect.h, themes.border);

    t = fontMgr_.renderText(renderer, label, selected ? themes.bg : white,
                            rowFontSize_, &tw, &th);
    if (t) {
      SDL_Rect tr = {lx + 10, y + (22 - th) / 2, tw, th};
      SDL_RenderCopy(renderer, t, nullptr, &tr);
      MemoryMonitor::getInstance().destroyTexture(t);
    }
    y += 24;
  };

  renderButton("All Spots", Filter::ALL, allBtnRect_);
  renderButton("POTA Spots Only", Filter::POTA, potaBtnRect_);
  renderButton("SOTA Spots Only", Filter::SOTA, sotaBtnRect_);

  // Done Button
  int doneW = 60, doneH = 24;
  doneBtnRect_ = {cx - doneW / 2, y_ + height_ - doneH - 6, doneW, doneH};
  RenderUtils::drawRect(renderer, (float)doneBtnRect_.x, (float)doneBtnRect_.y,
                        (float)doneBtnRect_.w, (float)doneBtnRect_.h,
                        themes.success);
  RenderUtils::drawRect(renderer, (float)doneBtnRect_.x, (float)doneBtnRect_.y,
                        (float)doneBtnRect_.w, (float)doneBtnRect_.h,
                        themes.border);
  t = fontMgr_.renderText(renderer, "Done", themes.bg, rowFontSize_, &tw, &th);
  if (t) {
    SDL_Rect tr = {doneBtnRect_.x + (doneW - tw) / 2,
                   doneBtnRect_.y + (doneH - th) / 2, tw, th};
    SDL_RenderCopy(renderer, t, nullptr, &tr);
    MemoryMonitor::getInstance().destroyTexture(t);
  }
}

bool ONTAPanel::handleSetupClick(int mx, int my) {
  if (mx >= allBtnRect_.x && mx < allBtnRect_.x + allBtnRect_.w &&
      my >= allBtnRect_.y && my < allBtnRect_.y + allBtnRect_.h) {
    pendingFilter_ = Filter::ALL;
    return true;
  }
  if (mx >= potaBtnRect_.x && mx < potaBtnRect_.x + potaBtnRect_.w &&
      my >= potaBtnRect_.y && my < potaBtnRect_.y + potaBtnRect_.h) {
    pendingFilter_ = Filter::POTA;
    return true;
  }
  if (mx >= sotaBtnRect_.x && mx < sotaBtnRect_.x + sotaBtnRect_.w &&
      my >= sotaBtnRect_.y && my < sotaBtnRect_.y + sotaBtnRect_.h) {
    pendingFilter_ = Filter::SOTA;
    return true;
  }
  if (mx >= doneBtnRect_.x && mx < doneBtnRect_.x + doneBtnRect_.w &&
      my >= doneBtnRect_.y && my < doneBtnRect_.y + doneBtnRect_.h) {
    if (filter_ != pendingFilter_) {
      filter_ = pendingFilter_;
      auto data = store_->get();
      data.hasSelection = false;
      store_->set(data);
      rebuildRows(data);

      if (onFilterChanged_) {
        std::string fstr;
        switch (filter_) {
        case Filter::POTA:
          fstr = "pota";
          break;
        case Filter::SOTA:
          fstr = "sota";
          break;
        default:
          fstr = "all";
          break;
        }
        onFilterChanged_(fstr);
      }
    }
    showSetup_ = false;
    return true;
  }
  return false;
}
