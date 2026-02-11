#include "ListPanel.h"
#include "FontCatalog.h"

#include <algorithm>

ListPanel::ListPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     const std::string &title,
                     const std::vector<std::string> &rows)
    : Widget(x, y, w, h), fontMgr_(fontMgr), title_(title), rows_(rows) {}

void ListPanel::setRows(const std::vector<std::string> &rows) {
  rows_ = rows;
  destroyCache();
}

void ListPanel::destroyCache() {
  if (titleTex_) {
    SDL_DestroyTexture(titleTex_);
    titleTex_ = nullptr;
  }
  for (auto &rc : rowCache_) {
    if (rc.tex) {
      SDL_DestroyTexture(rc.tex);
      rc.tex = nullptr;
    }
  }
  rowCache_.clear();
}

void ListPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
  SDL_Rect border = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &border);

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  bool titleFontChanged = (titleFontSize_ != lastTitleFontSize_);
  bool rowFontChanged = (rowFontSize_ != lastRowFontSize_);

  // Title (centered, cyan)
  if (titleFontChanged || !titleTex_) {
    if (titleTex_) {
      SDL_DestroyTexture(titleTex_);
      titleTex_ = nullptr;
    }
    SDL_Color cyan = {0, 200, 255, 255};
    titleTex_ = fontMgr_.renderText(renderer, title_, cyan, titleFontSize_,
                                    &titleW_, &titleH_);
    lastTitleFontSize_ = titleFontSize_;
  }

  int curY = y_ + pad;
  if (titleTex_) {
    int tx = x_ + (width_ - titleW_) / 2;
    SDL_Rect dst = {tx, curY, titleW_, titleH_};
    SDL_RenderCopy(renderer, titleTex_, nullptr, &dst);
    curY += titleH_ + pad;
  }

  // Rebuild row cache on font change or row count change
  if (rowCache_.size() != rows_.size() || rowFontChanged) {
    for (auto &rc : rowCache_) {
      if (rc.tex) {
        SDL_DestroyTexture(rc.tex);
        rc.tex = nullptr;
      }
    }
    rowCache_.resize(rows_.size());
    for (auto &rc : rowCache_)
      rc.text.clear();
    lastRowFontSize_ = rowFontSize_;
  }

  if (rows_.empty())
    return;

  // Divide remaining space evenly among rows
  int remaining = (y_ + height_) - curY;
  int rowH =
      std::max(rowFontSize_ + 4, remaining / static_cast<int>(rows_.size()));

  SDL_Color rowColor = {200, 200, 200, 255};
  for (size_t i = 0; i < rows_.size(); ++i) {
    int rowY = curY + static_cast<int>(i) * rowH;
    if (rowY + rowH > y_ + height_)
      break;

    // Alternating stripe background
    if (i % 2 == 0) {
      SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 15, 15, 15, 255);
    }
    SDL_Rect stripe = {x_ + 1, rowY, width_ - 2, rowH};
    SDL_RenderFillRect(renderer, &stripe);

    // Render row text (cached)
    if (rows_[i] != rowCache_[i].text) {
      if (rowCache_[i].tex) {
        SDL_DestroyTexture(rowCache_[i].tex);
        rowCache_[i].tex = nullptr;
      }
      rowCache_[i].tex =
          fontMgr_.renderText(renderer, rows_[i], rowColor, rowFontSize_,
                              &rowCache_[i].w, &rowCache_[i].h);
      rowCache_[i].text = rows_[i];
    }
    if (rowCache_[i].tex) {
      int ty = rowY + (rowH - rowCache_[i].h) / 2;
      SDL_Rect dst = {x_ + pad, ty, rowCache_[i].w, rowCache_[i].h};
      SDL_RenderCopy(renderer, rowCache_[i].tex, nullptr, &dst);
    }
  }
}

void ListPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::Fast);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  destroyCache();
}
