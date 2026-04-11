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

  renderChrome(renderer);

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
    // Dynamic height if the title is larger than default 20px
    titleAreaH_ = std::max(20, titleH_ + 10); 
  }

  // Start rows below title area
  contentY_ = y_ + titleAreaH_;
  int curY = contentY_;

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

  // Divide remaining space evenly among rows (accounting for any footer/legend)
  int remaining = (y_ + height_ - footerH_) - curY;
  rowH_ = std::max(rowFontSize_ + 4, remaining / static_cast<int>(rows_.size()));
  int rowH = rowH_;

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
