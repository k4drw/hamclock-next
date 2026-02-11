#include "SpaceWeatherPanel.h"
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
}

void SpaceWeatherPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "Awaiting data...", x_ + 8,
                      y_ + height_ / 2 - 8, {180, 180, 180, 255},
                      labelFontSize_);
    return;
  }

  bool labelFontChanged = (labelFontSize_ != lastLabelFontSize_);
  bool valueFontChanged = (valueFontSize_ != lastValueFontSize_);

  // 2x2 grid layout
  int cellW = width_ / 2;
  int cellH = height_ / 2;
  int pad = std::max(2, static_cast<int>(cellW * 0.06f));

  SDL_Color labelColor = {140, 140, 140, 255};

  for (int i = 0; i < kNumItems; ++i) {
    int col = i % 2;
    int row = i / 2;
    int cellX = x_ + col * cellW;
    int cellY = y_ + row * cellH;

    // Label (cached until font size changes)
    if (labelFontChanged || !items_[i].labelTex) {
      if (items_[i].labelTex) {
        SDL_DestroyTexture(items_[i].labelTex);
        items_[i].labelTex = nullptr;
      }
      items_[i].labelTex = fontMgr_.renderText(
          renderer, items_[i].label, labelColor, labelFontSize_,
          &items_[i].labelW, &items_[i].labelH);
    }

    // Value (re-render on data or font change, or color change)
    bool colorChanged =
        std::memcmp(&items_[i].valueColor, &items_[i].lastValueColor,
                    sizeof(SDL_Color)) != 0;
    if (items_[i].value != items_[i].lastValue || valueFontChanged ||
        colorChanged) {
      if (items_[i].valueTex) {
        SDL_DestroyTexture(items_[i].valueTex);
        items_[i].valueTex = nullptr;
      }
      items_[i].valueTex = fontMgr_.renderText(
          renderer, items_[i].value, items_[i].valueColor, valueFontSize_,
          &items_[i].valueW, &items_[i].valueH);
      items_[i].lastValue = items_[i].value;
      items_[i].lastValueColor = items_[i].valueColor;
    }

    // Draw label (top of cell, centered)
    if (items_[i].labelTex) {
      int lx = cellX + (cellW - items_[i].labelW) / 2;
      int ly = cellY + pad;
      SDL_Rect dst = {lx, ly, items_[i].labelW, items_[i].labelH};
      SDL_RenderCopy(renderer, items_[i].labelTex, nullptr, &dst);
    }

    // Draw value (below label, centered)
    if (items_[i].valueTex) {
      int vx = cellX + (cellW - items_[i].valueW) / 2;
      int vy = cellY + pad + items_[i].labelH + pad / 2;
      SDL_Rect dst = {vx, vy, items_[i].valueW, items_[i].valueH};
      SDL_RenderCopy(renderer, items_[i].valueTex, nullptr, &dst);
    }
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
