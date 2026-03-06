#include "ListPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"

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
    MemoryMonitor::getInstance().destroyTexture(titleTex_);
  }
  for (auto &rc : rowCache_) {
    if (rc.tex) {
      MemoryMonitor::getInstance().destroyTexture(rc.tex);
    }
  }
  rowCache_.clear();
}

void ListPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Background
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect bg = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &bg);

  // Draw pane border
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_Rect rect = {x_, y_, width_, height_}; // Renamed from 'rect' to 'bg' for
                                             // fill, keeping 'rect' for border
  SDL_RenderDrawRect(renderer, &rect);

  bool titleFontChanged = (titleFontSize_ != lastTitleFontSize_);
  bool rowFontChanged = (rowFontSize_ != lastRowFontSize_);

  // Title (standardized position and style)
  if (titleFontChanged || !titleTex_) {
    if (titleTex_) {
      MemoryMonitor::getInstance().destroyTexture(titleTex_);
    }
    SDL_Color cyan = themes.accent;
    titleTex_ = fontMgr_.catalog()->renderText(
        renderer, title_, cyan, FontStyle::MicroBold, &titleW_, &titleH_);
    lastTitleFontSize_ = fontMgr_.catalog()->ptSize(FontStyle::MicroBold);
  }

  if (titleTex_) {
    SDL_Rect dst = {x_ + 10, y_ + 5, titleW_, titleH_};
    SDL_RenderCopy(renderer, titleTex_, nullptr, &dst);
  }

  // Start rows below title area
  int titleAreaH = 20;
  int curY = y_ + titleAreaH;

  // Rebuild row cache on font change or row count change
  if (rowCache_.size() != rows_.size() || rowFontChanged) {
    for (auto &rc : rowCache_) {
      if (rc.tex) {
        MemoryMonitor::getInstance().destroyTexture(rc.tex);
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

  SDL_Color rowColor = themes.text;
  for (size_t i = 0; i < rows_.size(); ++i) {
    int rowY = curY + static_cast<int>(i) * rowH;
    if (rowY + rowH > y_ + height_)
      break;

    // Alternating stripe background, or accent for highlight
    SDL_Color stripeColor;
    if (static_cast<int>(i) == highlightedIndex_) {
      stripeColor = themes.accent;
      stripeColor.a = 180; // slightly transparent
    } else {
      stripeColor = (i % 2 == 0) ? themes.rowStripe1 : themes.rowStripe2;
    }
    RenderUtils::drawRect(renderer, x_ + 1, rowY, width_ - 2, rowH,
                          stripeColor);

    // Render row text (via overridable method)
    SDL_Color thisRowColor = getRowColor(static_cast<int>(i), rowColor);
    renderRowText(renderer, static_cast<int>(i), x_ + 1, rowY, width_ - 2, rowH,
                  thisRowColor);
  }
}

void ListPanel::renderRowText(SDL_Renderer *renderer, int index, int rx, int ry,
                              int rw, int rh, SDL_Color color) {
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  auto &rc = rowCache_[index];
  bool colorChanged =
      (rc.color.r != color.r || rc.color.g != color.g || rc.color.b != color.b);

  if (rows_[index] != rc.text || colorChanged) {
    if (rc.tex) {
      MemoryMonitor::getInstance().destroyTexture(rc.tex);
      rc.tex = nullptr;
    }
    rc.tex = fontMgr_.renderText(renderer, rows_[index], color, rowFontSize_,
                                 &rc.w, &rc.h);
    rc.text = rows_[index];
    rc.color = color;
  }

  if (rc.tex) {
    int tx = rx + pad;
    int ty = ry + (rh - rc.h) / 2;
    SDL_Rect dst = {tx, ty, rc.w, rc.h};
    SDL_RenderCopy(renderer, rc.tex, nullptr, &dst);
  }
}

void ListPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  titleFontSize_ = cat->ptSize(FontStyle::MicroBold);
  rowFontSize_ = cat->ptSize(FontStyle::Fast);
  destroyCache();
}

nlohmann::json ListPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  j["title"] = title_;
  j["rows"] = rows_;
  return j;
}
