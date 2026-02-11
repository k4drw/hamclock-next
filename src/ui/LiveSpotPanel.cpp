#include "LiveSpotPanel.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cstring>

LiveSpotPanel::LiveSpotPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<LiveSpotDataStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void LiveSpotPanel::update() {
  auto data = store_->get();
  if (!data.valid)
    return;

  // Track selected bands (for visual highlight, no texture rebuild needed)
  std::memcpy(lastSelected_, data.selectedBands, sizeof(lastSelected_));

  // Check if counts changed
  bool changed = !dataValid_ || std::memcmp(data.bandCounts, lastCounts_,
                                            sizeof(lastCounts_)) != 0;
  if (changed) {
    std::memcpy(lastCounts_, data.bandCounts, sizeof(lastCounts_));
    dataValid_ = true;
    // Count textures will be rebuilt in render()
    for (auto &bc : bandCache_) {
      if (bc.countTex) {
        SDL_DestroyTexture(bc.countTex);
        bc.countTex = nullptr;
      }
      bc.lastCount = -1;
    }
    // Rebuild subtitle if grid changed
    std::string sub = "of " + data.grid + " - PSK " +
                      std::to_string(data.windowMinutes) + " mins";
    if (sub != lastSubtitle_) {
      if (subtitleTex_) {
        SDL_DestroyTexture(subtitleTex_);
        subtitleTex_ = nullptr;
      }
      lastSubtitle_ = sub;
    }
  }
}

void LiveSpotPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  bool titleFontChanged = (titleFontSize_ != lastTitleFontSize_);
  bool cellFontChanged = (cellFontSize_ != lastCellFontSize_);

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color cyan = {0, 200, 255, 255};
  SDL_Color blue = {100, 100, 255, 255};

  int pad = 2;
  int curY = y_ + pad;

  // --- Title: "Live Spots" (centered, blue) ---
  if (titleFontChanged || !titleTex_) {
    if (titleTex_) {
      SDL_DestroyTexture(titleTex_);
      titleTex_ = nullptr;
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
        SDL_DestroyTexture(bandCache_[i].labelTex);
        bandCache_[i].labelTex = nullptr;
      }
      if (bandCache_[i].countTex) {
        SDL_DestroyTexture(bandCache_[i].countTex);
        bandCache_[i].countTex = nullptr;
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

    // Colored background
    const auto &bd = kBands[i];
    SDL_SetRenderDrawColor(renderer, bd.color.r, bd.color.g, bd.color.b, 255);
    SDL_Rect cellRect = {cx + gap, cy + gap, colW - 2 * gap, cellH - 2 * gap};
    SDL_RenderFillRect(renderer, &cellRect);

    // White border on selected bands (toggled for map plotting)
    if (lastSelected_[i]) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
      SDL_RenderDrawRect(renderer, &cellRect);
    }

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
        SDL_DestroyTexture(bandCache_[i].countTex);
        bandCache_[i].countTex = nullptr;
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
      SDL_DestroyTexture(footerTex_);
      footerTex_ = nullptr;
    }
    footerTex_ = fontMgr_.renderText(renderer, "Counts", white, cellFontSize_,
                                     &footerW_, &footerH_);
  }
  if (footerTex_) {
    int fy = gridBottom + (footerH - footerH_) / 2;
    SDL_Rect dst = {x_ + (width_ - footerW_) / 2, fy, footerW_, footerH_};
    SDL_RenderCopy(renderer, footerTex_, nullptr, &dst);
  }
}

void LiveSpotPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  if (cat) {
    // Title slightly larger than cell text
    titleFontSize_ = std::max(8, cat->ptSize(FontStyle::Fast) + 4);
    cellFontSize_ = cat->ptSize(FontStyle::Fast);
  }
  destroyCache();
}

bool LiveSpotPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (mx < x_ || mx >= x_ + width_ || my < y_ || my >= y_ + height_)
    return false;

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
  return true;
}

void LiveSpotPanel::destroyCache() {
  if (titleTex_) {
    SDL_DestroyTexture(titleTex_);
    titleTex_ = nullptr;
  }
  if (subtitleTex_) {
    SDL_DestroyTexture(subtitleTex_);
    subtitleTex_ = nullptr;
  }
  if (footerTex_) {
    SDL_DestroyTexture(footerTex_);
    footerTex_ = nullptr;
  }
  for (auto &bc : bandCache_) {
    if (bc.labelTex) {
      SDL_DestroyTexture(bc.labelTex);
      bc.labelTex = nullptr;
    }
    if (bc.countTex) {
      SDL_DestroyTexture(bc.countTex);
      bc.countTex = nullptr;
    }
    bc.lastCount = -1;
  }
  lastTitleFontSize_ = 0;
  lastCellFontSize_ = 0;
  lastSubtitle_.clear();
}
