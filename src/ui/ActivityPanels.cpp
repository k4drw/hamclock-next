#include "ActivityPanels.h"
#include "WidgetRegistry.h"
#include "../core/LiveSpotData.h" // For kBands and freqToBandIndex
#include "../core/Astronomy.h"
#include "../core/MemoryMonitor.h"
#include "../core/StringUtils.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h" // ADDED
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

static double ontaHaversineKm(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kR  = 6371.0;
  auto toRad = [](double d) { return d * 3.14159265358979323846 / 180.0; };
  double dLat = toRad(lat2 - lat1);
  double dLon = toRad(lon2 - lon1);
  double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
             std::cos(toRad(lat1)) * std::cos(toRad(lat2)) *
             std::sin(dLon / 2) * std::sin(dLon / 2);
  return 2.0 * kR * std::asin(std::sqrt(a));
}

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
  if (data->lastUpdated != lastUpdate_) {
    allRows_.clear();
    allPeds_.clear();
    auto now = std::chrono::system_clock::now();
    for (const auto &de : data->dxpeds) {
      if (now >= de.startTime && now <= de.endTime) {
        std::stringstream ss;
        ss << de.call << '\t' << de.location;
        allRows_.push_back(ss.str());
        allPeds_.push_back(de);
      }
    }
    if (allRows_.empty() && data->valid) {
      allRows_.push_back("No active expeditions");
    }
    // Clamp scroll and rebuild visible slice
    int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
    scrollOffset_ = std::min(scrollOffset_, maxScroll);
    int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allRows_.size());
    setRows(std::vector<std::string>(allRows_.begin() + scrollOffset_,
                                     allRows_.begin() + end));
    int pedEnd = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allPeds_.size());
    currentPeds_ =
        (scrollOffset_ < (int)allPeds_.size())
            ? std::vector<DXPedition>(allPeds_.begin() + scrollOffset_,
                                      allPeds_.begin() + pedEnd)
            : std::vector<DXPedition>{};
    // Restore highlight after data refresh if the selected call is still visible.
    int hi = -1;
    if (!selectedCall_.empty()) {
      for (size_t i = 0; i < currentPeds_.size(); ++i) {
        if (currentPeds_[i].call == selectedCall_) {
          hi = static_cast<int>(i);
          break;
        }
      }
    }
    setHighlightedIndex(hi);
    lastUpdate_ = data->lastUpdated;
  }
}

bool DXPedPanel::onMouseWheel(int scrollY) {
  if (allRows_.empty())
    return false;

  int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
  int newOffset = scrollOffset_ - scrollY;
  if (newOffset < 0)
    newOffset = 0;
  if (newOffset > maxScroll)
    newOffset = maxScroll;

  if (newOffset != scrollOffset_) {
    scrollOffset_ = newOffset;
    int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allRows_.size());
    setRows(std::vector<std::string>(allRows_.begin() + scrollOffset_,
                                     allRows_.begin() + end));
    int pedEnd = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allPeds_.size());
    currentPeds_ =
        (scrollOffset_ < (int)allPeds_.size())
            ? std::vector<DXPedition>(allPeds_.begin() + scrollOffset_,
                                      allPeds_.begin() + pedEnd)
            : std::vector<DXPedition>{};
    int hi = -1;
    if (!selectedCall_.empty()) {
      for (size_t i = 0; i < currentPeds_.size(); ++i) {
        if (currentPeds_[i].call == selectedCall_) {
          hi = static_cast<int>(i);
          break;
        }
      }
    }
    setHighlightedIndex(hi);
    return true;
  }
  return false;
}

bool DXPedPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  (void)mod;
  (void)clicks;
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int titleAreaH = pad * 2;
  if (titleTex_)
    titleAreaH += titleH_;

  if (my <= y_ + titleAreaH)
    return false;

  int rowY = my - (y_ + titleAreaH);
  int remaining = (y_ + height_) - (y_ + titleAreaH);
  int rowCount = static_cast<int>(currentPeds_.size());
  if (rowCount <= 0)
    return false;
  int rowH = std::max(rowFontSize_ + 4, remaining / rowCount);
  if (rowH <= 0)
    return false;
  size_t idx = rowY / rowH;
  if (idx >= currentPeds_.size())
    return false;

  const auto &clicked = currentPeds_[idx];
  // Unresolved rows: dimmed and inert.
  if (clicked.lat == 0.0 && clicked.lon == 0.0)
    return false;

  if (selectedCall_ == clicked.call) {
    selectedCall_.clear();
    setHighlightedIndex(-1);
    if (onPeditionDeactivated_)
      onPeditionDeactivated_();
  } else {
    selectedCall_ = clicked.call;
    setHighlightedIndex(static_cast<int>(idx));
    if (onPeditionActivated_)
      onPeditionActivated_(clicked);
  }
  return true;
}

void DXPedPanel::renderRowText(SDL_Renderer *renderer, int index, int rx, int ry,
                               int rw, int rh, SDL_Color color) {
  auto *cat = fontMgr_.catalog();
  if (!cat || index < 0 || index >= static_cast<int>(currentPeds_.size()))
    return;

  const auto &pe = currentPeds_[index];
  const std::string &call = pe.call;
  const std::string &loc = pe.location;

  SDL_Color rowColor = color;
  ThemeColors themes = getThemeColors(theme_);

  // Last day highlight logic: use warning color if ending within 24 hours and not selected.
  auto now = std::chrono::system_clock::now();
  auto hoursLeft = std::chrono::duration_cast<std::chrono::hours>(pe.endTime - now).count();
  if (hoursLeft < 24 && hoursLeft >= 0 && index != getHighlightedIndex()) {
    rowColor = themes.warning;
  }

  // Dim rows whose callsign could not be geolocated (lat/lon still at default 0).
  if (pe.lat == 0.0 && pe.lon == 0.0) {
    rowColor = {static_cast<Uint8>(rowColor.r / 2),
                static_cast<Uint8>(rowColor.g / 2),
                static_cast<Uint8>(rowColor.b / 2), rowColor.a};
  }

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int midY = ry + rh / 2;

  if (width_ > 300) {
    // Maximized/Wide Layout: 3 Columns (Call, Loc, Dates)
    int col1X = rx + pad;
    int col2X = rx + rw * 25 / 100;
    int col3X = rx + rw * 70 / 100;

    cat->drawText(renderer, call, col1X, midY, rowColor, FontStyle::Fast, false,
                  false, true);
    cat->drawText(renderer, loc, col2X, midY, rowColor, FontStyle::Fast, false,
                  false, true);

    auto startT = std::chrono::system_clock::to_time_t(pe.startTime);
    auto endT = std::chrono::system_clock::to_time_t(pe.endTime);
    struct tm tmStart, tmEnd;
    Astronomy::portable_gmtime(&startT, &tmStart);
    Astronomy::portable_gmtime(&endT, &tmEnd);

    char sBuf[16], eBuf[16], dateRange[64];
    std::strftime(sBuf, sizeof(sBuf), "%b %d", &tmStart);
    std::strftime(eBuf, sizeof(eBuf), "%b %d", &tmEnd);
    std::snprintf(dateRange, sizeof(dateRange), "%s - %s", sBuf, eBuf);

    cat->drawText(renderer, dateRange, col3X, midY, rowColor, FontStyle::Fast,
                  false, false, true);
  } else {
    // Standard layout: 2 Columns
    int colX = rx + rw * 42 / 100;
    cat->drawText(renderer, call, rx + pad, midY, rowColor, FontStyle::Fast,
                  false, false, true);
    cat->drawText(renderer, loc, colX, midY, rowColor, FontStyle::Fast, false,
                  false, true);
  }
}

// --- ONTAPanel ---

bool ONTAPanel::onMouseWheel(int scrollY) {
  if (allSpots_.empty())
    return false;
  int maxScroll = std::max(0, (int)allSpots_.size() - MAX_VISIBLE_ROWS);
  int newOffset = std::clamp(scrollOffset_ - scrollY, 0, maxScroll);
  if (newOffset == scrollOffset_)
    return false;
  scrollOffset_ = newOffset;
  int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allSpots_.size());
  currentSpots_ = std::vector<ONTASpot>(allSpots_.begin() + scrollOffset_,
                                         allSpots_.begin() + end);
  std::vector<std::string> rows(currentSpots_.size(), "");
  setRows(rows);
  return true;
}

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
  allSpots_.clear();
  for (const auto &os : data.ontaSpots) {
    if (filter_ == Filter::POTA && os.program != "POTA")
      continue;
    if (filter_ == Filter::SOTA && os.program != "SOTA")
      continue;
    if (maxDistKm_ > 0 && (os.lat != 0.0 || os.lon != 0.0)) {
      double dist = ontaHaversineKm(deLat_, deLon_, os.lat, os.lon);
      if (dist > maxDistKm_)
        continue;
    }
    if (activeBandFilter_ != -1) {
      if (freqToBandIndex(os.freqKhz) != activeBandFilter_)
        continue;
    }
    if (!activeModeFilter_.empty()) {
      if (os.mode != activeModeFilter_)
        continue;
    }
    allSpots_.push_back(os);
  }

  int maxScroll = std::max(0, (int)allSpots_.size() - MAX_VISIBLE_ROWS);
  scrollOffset_ = std::min(scrollOffset_, maxScroll);
  int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allSpots_.size());
  currentSpots_ = std::vector<ONTASpot>(allSpots_.begin() + scrollOffset_,
                                         allSpots_.begin() + end);

  std::vector<std::string> rows(currentSpots_.size(), "");
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
  if (data->lastUpdated != lastUpdate_) {
    rebuildRows(*data);
    lastUpdate_ = data->lastUpdated;
  }

  // Update highlight from selection
  if (data->hasSelection) {
    int foundIdx = -1;
    for (size_t i = 0; i < currentSpots_.size(); ++i) {
      if (currentSpots_[i].call == data->selectedSpot.call &&
          currentSpots_[i].ref == data->selectedSpot.ref) {
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
  // Detect double height (SidePanel full height is ~332)
  legendH_ = (height_ > 300) ? 28 : 0;
  modeFilterH_ = (height_ > 300) ? 14 : 0;

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
  auto *cat = fontMgr_.catalog();
  // Measure chip label width without allocating a GPU texture on every frame.
  int cw = fontMgr_.getLogicalWidth(chip, cat->ptSize(FontStyle::Fast));

  // Visual button background centered vertically in title area
  int btnH = 20;
  int btnY = y_ + (titleAreaH - btnH) / 2;
  int chipX = x_ + width_ - pad - cw - 5;
  SDL_Rect btnRect = {chipX - 5, btnY, cw + 10, btnH};
  SDL_Color chipColor = (filter_ != Filter::ALL) ? themes.accent : themes.info;

  // Render concept: Semi-transparent black background + border matching text
  // color
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_Color btnBg = themes.rowStripe1;
  SDL_SetRenderDrawColor(renderer, btnBg.r, btnBg.g, btnBg.b, 160);
  SDL_RenderFillRect(renderer, &btnRect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 255);
  SDL_RenderDrawRect(renderer, &btnRect);

  cat->drawText(renderer, chip, btnRect.x + btnRect.w / 2,
                btnRect.y + btnRect.h / 2, chipColor, FontStyle::Fast, true,
                false, true);

  // Generous hit box spans full title height
  chipRect_ = {btnRect.x, y_, btnRect.w, titleAreaH};

  // Render Band Legend and Mode Filter at bottom if double-height
  if (legendH_ > 0) {
    renderBandLegend(renderer, y_ + height_ - 2);
  }
  if (modeFilterH_ > 0) {
    renderModeFilter(renderer, y_ + height_ - 2 - legendH_);
  }

  if (currentSpots_.empty()) {
    return;
  }

  // Calculate row height, accounting for legend and mode filter if present
  int curY = y_ + titleAreaH;
  int remaining = (y_ + height_ - legendH_ - modeFilterH_) - curY;
  int rowCount = static_cast<int>(currentSpots_.size());
  int rowH = std::max(rowFontSize_ + 4, remaining / rowCount);

  // P/S column only shown in ALL mode; in POTA/SOTA the space goes to Ref
  bool showProg = (filter_ == Filter::ALL);

  // Column layout: Mode | Call | Ref | [P/S in ALL mode]
  // Use "SSB" (widest mode) and "WB2LJK/P" (wide portable call) as anchors
  int modeX = x_ + pad;
  int callX = modeX + fontMgr_.getLogicalWidth("SSB", rowFontSize_) + 8;
  int refX = callX + fontMgr_.getLogicalWidth("WB2LJK/P", rowFontSize_) + 6;
  int progXEnd = x_ + width_ - pad;
  // Ref extends to full right edge when P/S column is hidden
  int refXEnd =
      showProg ? (progXEnd - fontMgr_.getLogicalWidth("S", rowFontSize_) - 4)
               : progXEnd;

  for (size_t i = 0; i < currentSpots_.size(); ++i) {
    if (i >= spotCache_.size())
      break;

    int rowY = curY + static_cast<int>(i) * rowH;
    const auto &spot = currentSpots_[i];
    auto &cache = spotCache_[i];
    SDL_Color color = getRowColor(static_cast<int>(i), getThemeColors(theme_).text);

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
      SDL_Rect clip = {modeX, rowY, callX - modeX - 2, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.modeTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
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
      SDL_Rect clip = {callX, rowY, refX - callX - 2, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.callTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
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
      SDL_Rect clip = {refX, rowY, refXEnd - refX, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.refTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
    }

    // 4. Program (P/S) — only in ALL mode
    if (showProg) {
      std::string shortProg =
          spot.program.empty() ? "" : spot.program.substr(0, 1);
      if (!cache.progTex || cache.lastProg != shortProg) {
        if (cache.progTex)
          MemoryMonitor::getInstance().destroyTexture(cache.progTex);
        cache.progTex =
            fontMgr_.renderText(renderer, shortProg, color, rowFontSize_,
                                &cache.progW, &cache.progH);
        cache.lastProg = shortProg;
      }
      if (cache.progTex) {
        int ty = rowY + (rowH - cache.progH) / 2;
        SDL_Rect dst = {progXEnd - cache.progW, ty, cache.progW, cache.progH};
        SDL_RenderCopy(renderer, cache.progTex, nullptr, &dst);
      }
    }
  }
}

bool ONTAPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
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

    // Clear selection if it no longer matches the new filter
    auto data = store_->get();
    if (data->hasSelection) {
      std::string lowerProg = StringUtils::toLower(data->selectedSpot.program);
      bool match = (filter_ == Filter::ALL) ||
                   (filter_ == Filter::POTA && lowerProg == "pota") ||
                   (filter_ == Filter::SOTA && lowerProg == "sota");
      if (!match) {
        ActivityData newData = *data;
        newData.hasSelection = false;
        store_->set(newData);
      }
    }

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

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int titleAreaH = pad * 2;
  if (titleTex_)
    titleAreaH += titleH_;

  // Mode filter row (above band legend)
  if (modeFilterH_ > 0) {
    int modeY = y_ + height_ - 2 - legendH_ - modeFilterH_;
    if (my >= modeY && my < modeY + modeFilterH_) {
      static const char *kModes[] = {"CW", "SSB", "FT8", "FT4", "RTTY", "WSPR"};
      int cols = 6;
      int cellW = (width_ - 4) / cols;
      int col = (mx - (x_ + 4)) / cellW;
      if (col >= 0 && col < cols) {
        const char *clicked = kModes[col];
        activeModeFilter_ = (activeModeFilter_ == clicked) ? "" : clicked;
        
        auto data = store_->get();
        ActivityData newData = *data;
        newData.activeModeFilter = activeModeFilter_;
        store_->set(newData);
        
        lastUpdate_ = std::chrono::system_clock::time_point{}; // force update
        update();
        return true;
      }
    }
  }

  // Check band legend clicks
  if (my >= y_ + height_ - legendH_ && legendH_ > 0) {
    int cellH = 14;
    int cols = 6;
    int cellW = (width_ - 4) / cols;
    int legY = y_ + height_ - legendH_;
    int row = (my - legY) / cellH;
    int col = (mx - (x_ + 4)) / cellW;
    if (row >= 0 && row < 2 && col >= 0 && col < cols) {
      int bandIdx = row * cols + col;
      if (bandIdx >= 0 && bandIdx < kNumBands) {
        if (activeBandFilter_ == bandIdx)
          activeBandFilter_ = -1;
        else
          activeBandFilter_ = bandIdx;
          
        auto data = store_->get();
        ActivityData newData = *data;
        newData.activeBandFilter = activeBandFilter_;
        store_->set(newData);
          
        lastUpdate_ = std::chrono::system_clock::time_point{}; // force update
        update();
        return true;
      }
    }
  }

  // Title area clicks (outside the chip) are intentionally not consumed here
  // so they bubble up to PaneContainer for widget selection menu.

  if (my > y_ + titleAreaH) {
    int rowY = my - (y_ + titleAreaH);
    int remaining = (y_ + height_ - legendH_ - modeFilterH_) - (y_ + titleAreaH);
    int rowCount = static_cast<int>(currentSpots_.size());
    int rowH = (rowCount > 0) ? std::max(rowFontSize_ + 4, remaining / rowCount)
                              : rowFontSize_ + 4;
    if (rowH > 0) {
      size_t idx = rowY / rowH;
      if (idx < currentSpots_.size()) {
        auto data = store_->get();
        const auto &clicked = currentSpots_[idx];
        ActivityData newData = *data;
        bool isSame = data->hasSelection &&
                      data->selectedSpot.call == clicked.call &&
                      data->selectedSpot.ref == clicked.ref;
        if (isSame) {
          newData.hasSelection = false;
          store_->set(newData);
          setHighlightedIndex(-1);
          if (onSpotDeactivated_)
            onSpotDeactivated_();
        } else {
          newData.hasSelection = true;
          newData.selectedSpot = clicked;
          store_->set(newData);
          setHighlightedIndex(static_cast<int>(idx));
          if (onSpotActivated_)
            onSpotActivated_(clicked);
        }
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
  auto *cat = fontMgr_.catalog();
  // Background
  SDL_Rect bg = {x_, y_, width_, height_};
  RenderUtils::drawRect(renderer, (float)bg.x, (float)bg.y, (float)bg.w,
                        (float)bg.h, themes.bg);
  RenderUtils::drawRectOutline(renderer, (float)bg.x, (float)bg.y, (float)bg.w,
                               (float)bg.h, themes.border);

  SDL_Color cyan = themes.accent;
  SDL_Color white = themes.text;

  int y = y_ + 10;
  int cx = x_ + width_ / 2;

  // Title
  cat->drawText(renderer, "--- ONTA Filter ---", cx, y, cyan,
                FontStyle::MicroBold, true);
  y += 22;
  int lx = x_ + 10;
  int btnW = width_ - 20;
  int btnH = std::min(22, (height_ - 60) / 4);

  auto renderButton = [&](const char *label, Filter f, SDL_Rect &rect) {
    bool selected = (pendingFilter_ == f);
    rect = {lx, y, btnW, btnH};
    RenderUtils::drawRect(renderer, (float)rect.x, (float)rect.y, (float)rect.w,
                          (float)rect.h,
                          selected ? themes.accent : themes.rowStripe1);
    RenderUtils::drawRectOutline(renderer, (float)rect.x, (float)rect.y,
                                 (float)rect.w, (float)rect.h, themes.border);

    cat->drawText(renderer, label, rect.x + rect.w / 2, y + btnH / 2,
                  selected ? themes.bg : white, FontStyle::Fast, true, false,
                  true);
    y += btnH + 2;
  };

  renderButton("All Spots", Filter::ALL, allBtnRect_);
  renderButton("POTA Spots Only", Filter::POTA, potaBtnRect_);
  renderButton("SOTA Spots Only", Filter::SOTA, sotaBtnRect_);

  // Distance filter row
  y += 4;
  int arrowW = btnH;
  int valW = btnW - arrowW * 2 - 4;
  distDecBtn_ = {lx, y, arrowW, btnH};
  SDL_Rect valRect = {lx + arrowW + 2, y, valW, btnH};
  distIncBtn_ = {lx + arrowW + 2 + valW + 2, y, arrowW, btnH};

  RenderUtils::drawRect(renderer, (float)distDecBtn_.x, (float)distDecBtn_.y,
                        (float)distDecBtn_.w, (float)distDecBtn_.h,
                        themes.rowStripe2);
  RenderUtils::drawRectOutline(renderer, (float)distDecBtn_.x,
                               (float)distDecBtn_.y, (float)distDecBtn_.w,
                               (float)distDecBtn_.h, themes.border);
  cat->drawText(renderer, "-", distDecBtn_.x + distDecBtn_.w / 2,
                y + btnH / 2, white, FontStyle::Fast, true, false, true);

  RenderUtils::drawRect(renderer, (float)valRect.x, (float)valRect.y,
                        (float)valRect.w, (float)valRect.h,
                        themes.rowStripe1);
  RenderUtils::drawRectOutline(renderer, (float)valRect.x, (float)valRect.y,
                               (float)valRect.w, (float)valRect.h,
                               themes.border);
  char distBuf[24];
  if (pendingMaxDistKm_ == 0)
    std::snprintf(distBuf, sizeof(distBuf), "No limit");
  else
    std::snprintf(distBuf, sizeof(distBuf), "%d km", pendingMaxDistKm_);
  cat->drawText(renderer, distBuf, valRect.x + valRect.w / 2,
                y + btnH / 2, themes.accent, FontStyle::Fast, true, false,
                true);

  RenderUtils::drawRect(renderer, (float)distIncBtn_.x, (float)distIncBtn_.y,
                        (float)distIncBtn_.w, (float)distIncBtn_.h,
                        themes.rowStripe2);
  RenderUtils::drawRectOutline(renderer, (float)distIncBtn_.x,
                               (float)distIncBtn_.y, (float)distIncBtn_.w,
                               (float)distIncBtn_.h, themes.border);
  cat->drawText(renderer, "+", distIncBtn_.x + distIncBtn_.w / 2,
                y + btnH / 2, white, FontStyle::Fast, true, false, true);
  y += btnH + 2;

  // Done Button
  int doneW = 80, doneH = 28;
  doneBtnRect_ = {cx - doneW / 2, y_ + height_ - doneH - 6, doneW, doneH};
  RenderUtils::drawRect(renderer, (float)doneBtnRect_.x, (float)doneBtnRect_.y,
                        (float)doneBtnRect_.w, (float)doneBtnRect_.h,
                        themes.success);
  RenderUtils::drawRectOutline(renderer, (float)doneBtnRect_.x,
                               (float)doneBtnRect_.y, (float)doneBtnRect_.w,
                               (float)doneBtnRect_.h, themes.border);
  cat->drawText(renderer, "Done", cx, doneBtnRect_.y + doneH / 2, themes.bg,
                FontStyle::Fast, true, false, true);
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
  if (distDecBtn_.w > 0 &&
      mx >= distDecBtn_.x && mx < distDecBtn_.x + distDecBtn_.w &&
      my >= distDecBtn_.y && my < distDecBtn_.y + distDecBtn_.h) {
    pendingMaxDistKm_ = std::max(0, pendingMaxDistKm_ - 100);
    return true;
  }
  if (distIncBtn_.w > 0 &&
      mx >= distIncBtn_.x && mx < distIncBtn_.x + distIncBtn_.w &&
      my >= distIncBtn_.y && my < distIncBtn_.y + distIncBtn_.h) {
    pendingMaxDistKm_ = std::min(5000, pendingMaxDistKm_ + 100);
    return true;
  }

  if (mx >= doneBtnRect_.x && mx < doneBtnRect_.x + doneBtnRect_.w &&
      my >= doneBtnRect_.y && my < doneBtnRect_.y + doneBtnRect_.h) {
    bool needRebuild = false;
    if (maxDistKm_ != pendingMaxDistKm_) {
      maxDistKm_ = pendingMaxDistKm_;
      needRebuild = true;
      if (onMaxDistChanged_)
        onMaxDistChanged_(maxDistKm_);
    }
    bool filterChanged = (filter_ != pendingFilter_);
    if (filterChanged) {
      filter_ = pendingFilter_;
      needRebuild = true;
    }
    if (needRebuild) {
      auto data = store_->get();
      ActivityData newData = *data;
      newData.hasSelection = false;
      store_->set(newData);
      rebuildRows(newData);
    }
    if (filterChanged && onFilterChanged_) {
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
    showSetup_ = false;
    return true;
  }
  return false;
}

void ONTAPanel::renderBandLegend(SDL_Renderer *renderer, int maxY) {
  int cellH = 14;           // Tiny font target 12px + 2px padding
  int legendH = cellH * 2;  // 28px for 2 rows
  int legendY = maxY - legendH;

  ThemeColors themes = getThemeColors(theme_);

  // Background for the legend area to prevent spot overlap (fully opaque)
  SDL_Rect legendRect = {x_ + 1, legendY, width_ - 2, legendH};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &legendRect);

  // Top border/separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 200);
  SDL_RenderDrawLine(renderer, x_ + 1, legendY, x_ + width_ - 1, legendY);

  int cols = 6;
  int cellW = (width_ - 4) / cols;
  int boxSize = 7;
  for (int i = 0; i < kNumBands; ++i) {
    int row = i / cols;
    int col = i % cols;
    int lx = x_ + 4 + col * cellW;
    int midY = legendY + row * cellH + cellH / 2;

    bool isSelected = (activeBandFilter_ == i);
    bool isDimmed = (activeBandFilter_ != -1 && !isSelected);
    Uint8 alpha = isDimmed ? 100 : 255;

    // Colored square, vertically centered in the row
    SDL_Rect box = {lx + 1, midY - boxSize / 2, boxSize, boxSize};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, kBands[i].color.r, kBands[i].color.g,
                           kBands[i].color.b, alpha);
    SDL_RenderFillRect(renderer, &box);

    if (isSelected) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_Rect border = {box.x - 1, box.y - 1, box.w + 2, box.h + 2};
      SDL_RenderDrawRect(renderer, &border);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Label right of box, vertically centered on the same midline
    std::string label(kBands[i].name);
    if (!label.empty() && label.back() == 'm')
      label.pop_back();
    SDL_Color tc = themes.text;
    tc.a = alpha;
    fontMgr_.catalog()->drawText(
        renderer, label, lx + boxSize + 1, midY, tc, FontStyle::Tiny,
        false, false, true);
  }
}

void ONTAPanel::renderModeFilter(SDL_Renderer *renderer, int maxY) {
  int cellH = 14;
  int rowY = maxY - cellH;
  ThemeColors themes = getThemeColors(theme_);

  // Background + separator
  SDL_Rect bg = {x_ + 1, rowY, width_ - 2, cellH};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &bg);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g, themes.border.b, 200);
  SDL_RenderDrawLine(renderer, x_ + 1, rowY, x_ + width_ - 1, rowY);

  static const char *kModes[] = {"CW", "SSB", "FT8", "FT4", "RTTY", "WSPR"};
  int cols = 6;
  int cellW = (width_ - 4) / cols;
  int boxSz = 7;
  int midY = rowY + cellH / 2;

  for (int i = 0; i < 6; ++i) {
    const char *mode = kModes[i];
    bool isSelected = (activeModeFilter_ == mode);
    bool isDimmed = (!activeModeFilter_.empty() && !isSelected);
    Uint8 alpha = isDimmed ? 100 : 255;

    int lx = x_ + 4 + i * cellW;
    // Just a generic color since there is no modeColor() available here
    SDL_Color mc = {150, 150, 150, 255}; 

    SDL_Rect box = {lx + 1, midY - boxSz / 2, boxSz, boxSz};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, mc.r, mc.g, mc.b, alpha);
    SDL_RenderFillRect(renderer, &box);
    if (isSelected) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_Rect border = {box.x - 1, box.y - 1, box.w + 2, box.h + 2};
      SDL_RenderDrawRect(renderer, &border);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_Color tc = {mc.r, mc.g, mc.b, alpha};
    fontMgr_.catalog()->drawText(renderer, mode, lx + boxSz + 1, midY, tc,
                                 FontStyle::Tiny, false, false, true);
  }
}

REGISTER_WIDGET("dx_peditions", "DX Peditions", true, false, {
  return std::make_unique<DXPedPanel>(
      0, 0, 0, 0, deps.fontMgr, *deps.activityProvider, deps.activityStore);
})

REGISTER_WIDGET("on_the_air", "On The Air", true, false, {
  auto p = std::make_unique<ONTAPanel>(
      0, 0, 0, 0, deps.fontMgr, *deps.activityProvider, deps.activityStore);
  p->setFilter(deps.appCfg.ontaFilter);
  p->setDeLocation(deps.appCfg.lat, deps.appCfg.lon);
  p->setMaxDistKm(deps.appCfg.ontaMaxDistKm);
  p->setOnFilterChanged([&appCfg = deps.appCfg, &cfgMgr = deps.cfgMgr](const std::string &f) {
    appCfg.ontaFilter = f;
    cfgMgr.save(appCfg);
  });
  p->setOnMaxDistChanged([&appCfg = deps.appCfg, &cfgMgr = deps.cfgMgr](int km) {
    appCfg.ontaMaxDistKm = km;
    cfgMgr.save(appCfg);
  });
  return p;
})
