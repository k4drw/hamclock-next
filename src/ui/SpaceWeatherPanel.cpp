#include "SpaceWeatherPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

SpaceWeatherPanel::SpaceWeatherPanel(int x, int y, int w, int h,
                                     FontManager &fontMgr,
                                     std::shared_ptr<SolarDataStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {
  items_[0].label = "SFI";
  items_[1].label = "SN";
  items_[2].label = "A";
  items_[3].label = "K";
  items_[4].label = "Wind";
  items_[5].label = "Dens";
  items_[6].label = "Bz";
  items_[7].label = "Bt";
  items_[8].label = "DST";
  items_[9].label = "Aurora";
  items_[10].label = "-";
  items_[11].label = "-";
}

SDL_Color SpaceWeatherPanel::colorForK(int k) {
  if (k < 3)
    return {0, 255, 0, 255}; // Green
  if (k <= 4)
    return {255, 255, 0, 255}; // Yellow
  return {255, 50, 50, 255};   // Red
}

SDL_Color SpaceWeatherPanel::colorForSFI(int sfi) {
  if (sfi > 100)
    return {0, 255, 0, 255}; // Green
  if (sfi > 70)
    return {255, 255, 0, 255}; // Yellow
  return {255, 50, 50, 255};   // Red
}

void SpaceWeatherPanel::destroyCache() {
  for (auto &item : items_) {
    if (item.labelTex) {
      SDL_DestroyTexture(item.labelTex);
      item.labelTex = nullptr;
    }
    if (item.valueTex) {
      SDL_DestroyTexture(item.valueTex);
      item.valueTex = nullptr;
    }
    item.lastValue.clear();
    item.lastValueColor = {0, 0, 0, 0};
  }
}

void SpaceWeatherPanel::update() {
  SolarData data = store_->get();
  dataValid_ = data.valid;
  if (!data.valid)
    return;

  char buf[16];

  std::snprintf(buf, sizeof(buf), "%d", data.sfi);
  items_[0].value = buf;
  items_[0].valueColor = colorForSFI(data.sfi);

  std::snprintf(buf, sizeof(buf), "%d", data.sunspot_number);
  items_[1].value = buf;
  items_[1].valueColor = {0, 255, 128, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.a_index);
  items_[2].value = buf;
  items_[2].valueColor = {255, 255, 255, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.k_index);
  items_[3].value = buf;
  items_[3].valueColor = colorForK(data.k_index);

  std::snprintf(buf, sizeof(buf), "%.0f", data.solar_wind_speed);
  items_[4].value = buf;
  items_[4].valueColor = {255, 128, 0, 255};

  std::snprintf(buf, sizeof(buf), "%.1f", data.solar_wind_density);
  items_[5].value = buf;
  items_[5].valueColor = {0, 200, 255, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.bz);
  items_[6].value = buf;
  items_[6].valueColor =
      (data.bz < 0) ? SDL_Color{255, 50, 50, 255} : SDL_Color{0, 255, 0, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.bt);
  items_[7].value = buf;
  items_[7].valueColor = {255, 255, 255, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.dst);
  items_[8].value = buf;
  items_[8].valueColor = (data.dst < -50) ? SDL_Color{255, 50, 50, 255}
                                          : SDL_Color{255, 255, 255, 255};

  std::snprintf(buf, sizeof(buf), "%d", data.aurora);
  items_[9].value = buf;
  items_[9].valueColor = (data.aurora > 50) ? SDL_Color{255, 128, 0, 255}
                                            : SDL_Color{0, 255, 255, 255};

  items_[10].value = "-";
  items_[11].value = "-";
}

void SpaceWeatherPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Cycle pages every 7 seconds
  uint32_t now = SDL_GetTicks();
  if (now - lastPageUpdate_ > 7000) {
    currentPage_ = (currentPage_ + 1) % 3;
    lastPageUpdate_ = now;
    destroyCache(); // Force redraw of new page items
  }

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "Awaiting data...", x_ + 8,
                      y_ + height_ / 2 - 8, themes.textDim, labelFontSize_);
    return;
  }

  bool labelFontChanged = (labelFontSize_ != lastLabelFontSize_);
  bool valueFontChanged = (valueFontSize_ != lastValueFontSize_);

  // 2x2 grid layout for current page
  int cellW = width_ / 2;
  int cellH = height_ / 2;
  int pad = std::max(2, static_cast<int>(cellH * 0.1f));

  SDL_Color labelColor = themes.textDim;

  int startIdx = currentPage_ * kItemsPerPage;
  for (int i = 0; i < kItemsPerPage; ++i) {
    int itemIdx = startIdx + i;
    int col = i % 2;
    int row = i / 2;
    int cellX = x_ + col * cellW;
    int cellY = y_ + row * cellH;

    // Label (cached until font size changes)
    if (labelFontChanged || !items_[itemIdx].labelTex) {
      if (items_[itemIdx].labelTex) {
        SDL_DestroyTexture(items_[itemIdx].labelTex);
        items_[itemIdx].labelTex = nullptr;
      }
      items_[itemIdx].labelTex = fontMgr_.renderText(
          renderer, items_[itemIdx].label, labelColor, labelFontSize_,
          &items_[itemIdx].labelW, &items_[itemIdx].labelH);
    }

    // Value (re-render on data or font change, or color change)
    bool colorChanged =
        std::memcmp(&items_[itemIdx].valueColor,
                    &items_[itemIdx].lastValueColor, sizeof(SDL_Color)) != 0;
    if (items_[itemIdx].value != items_[itemIdx].lastValue ||
        valueFontChanged || colorChanged) {
      if (items_[itemIdx].valueTex) {
        SDL_DestroyTexture(items_[itemIdx].valueTex);
        items_[itemIdx].valueTex = nullptr;
      }
      items_[itemIdx].valueTex = fontMgr_.renderText(
          renderer, items_[itemIdx].value, items_[itemIdx].valueColor,
          valueFontSize_, &items_[itemIdx].valueW, &items_[itemIdx].valueH);
      items_[itemIdx].lastValue = items_[itemIdx].value;
      items_[itemIdx].lastValueColor = items_[itemIdx].valueColor;
    }

    // Draw label (top of cell, centered)
    if (items_[itemIdx].labelTex) {
      int lx = cellX + (cellW - items_[itemIdx].labelW) / 2;
      int ly = cellY + pad;
      SDL_Rect dst = {lx, ly, items_[itemIdx].labelW, items_[itemIdx].labelH};
      SDL_RenderCopy(renderer, items_[itemIdx].labelTex, nullptr, &dst);
    }

    // Draw value (below label, centered)
    if (items_[itemIdx].valueTex) {
      int vx = cellX + (cellW - items_[itemIdx].valueW) / 2;
      int vy = cellY + pad + items_[itemIdx].labelH;
      SDL_Rect dst = {vx, vy, items_[itemIdx].valueW, items_[itemIdx].valueH};
      SDL_RenderCopy(renderer, items_[itemIdx].valueTex, nullptr, &dst);
    }
  }

  // Draw pagination dots (bottom center)
  for (int i = 0; i < 3; ++i) {
    SDL_Rect dot = {x_ + width_ / 2 - 15 + i * 12, y_ + height_ - 8, 6, 6};
    if (i == currentPage_) {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
    }
    SDL_RenderFillRect(renderer, &dot);
  }

  lastLabelFontSize_ = labelFontSize_;
  lastValueFontSize_ = valueFontSize_;
}

void SpaceWeatherPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::Fast);
  valueFontSize_ = cat->ptSize(FontStyle::SmallBold);
  destroyCache();
}
