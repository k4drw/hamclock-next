#include "BandConditionsPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <nlohmann/json.hpp>

BandConditionsPanel::BandConditionsPanel(
    int x, int y, int w, int h, FontManager &fontMgr,
    std::shared_ptr<BandConditionsStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)) {}

void BandConditionsPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;
}

SDL_Color BandConditionsPanel::colorForCondition(BandCondition cond) {
  switch (cond) {
  case BandCondition::EXCELLENT:
    return {0, 255, 255, 255}; // Cyan
  case BandCondition::GOOD:
    return {0, 255, 0, 255}; // Green
  case BandCondition::FAIR:
    return {255, 255, 0, 255}; // Yellow
  case BandCondition::POOR:
    return {255, 50, 50, 255}; // Red
  default:
    return {150, 150, 150, 255};
  }
}

const char *BandConditionsPanel::stringForCondition(BandCondition cond,
                                                    bool shortForm) const {
  if (shortForm) {
    switch (cond) {
    case BandCondition::EXCELLENT:
      return "E";
    case BandCondition::GOOD:
      return "G";
    case BandCondition::FAIR:
      return "F";
    case BandCondition::POOR:
      return "P";
    default:
      return "-";
    }
  }
  switch (cond) {
  case BandCondition::EXCELLENT:
    return "Exc";
  case BandCondition::GOOD:
    return "Good";
  case BandCondition::FAIR:
    return "Fair";
  case BandCondition::POOR:
    return "Poor";
  default:
    return "-";
  }
}

void BandConditionsPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

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
    fontMgr_.drawText(renderer, "No Data", x_ + width_ / 2, y_ + height_ / 2,
                      {150, 150, 150, 255}, tableFontSize_, false, true);
    return;
  }

  int pad = 4;
  int colWidth = (width_ - 2 * pad) / 3;
  int numRows =
      static_cast<int>(currentData_.statuses.size()) + 1; // +1 for header
  int rowHeight = (height_ - 2 * pad) / numRows;

  // Dynamically scale font if rowHeight is too small
  int dynamicFontSize = tableFontSize_;
  if (rowHeight < dynamicFontSize + 4) {
    dynamicFontSize = std::max(8, rowHeight - 4);
  }

  SDL_Color headerColor = themes.accent;
  SDL_Color labelColor = themes.text;

  int curY = y_ + pad;

  auto drawInCol = [&](const std::string &text, int colIdx, SDL_Color color,
                       int fontSize, bool bold = false) {
    int tx = x_ + pad + colIdx * colWidth + colWidth / 2;
    int ty = curY + rowHeight / 2;
    fontMgr_.drawText(renderer, text, tx, ty, color, fontSize, bold, true);
  };

  // Header
  bool useShort = (width_ < 100);
  if (!useShort) {
    drawInCol("Band", 0, headerColor, dynamicFontSize, true);
    drawInCol("Day", 1, headerColor, dynamicFontSize, true);
    drawInCol("Night", 2, headerColor, dynamicFontSize, true);
  } else {
    // Very narrow: no header or just one row?
    // Usually original HamClock narrow Prop has no header or just vertical
    // bands.
  }

  // Line under header (only if showing header)
  if (!useShort) {
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, themes.border.a);
    SDL_RenderDrawLine(renderer, x_ + pad, curY + rowHeight - 2,
                       x_ + width_ - pad, curY + rowHeight - 2);
    curY += rowHeight;
  }

  // Rows
  for (const auto &status : currentData_.statuses) {
    drawInCol(status.band, 0, labelColor, dynamicFontSize);
    drawInCol(stringForCondition(status.day, useShort), 1,
              colorForCondition(status.day), dynamicFontSize);
    drawInCol(stringForCondition(status.night, useShort), 2,
              colorForCondition(status.night), dynamicFontSize);
    curY += rowHeight;
  }
}

void BandConditionsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  // Use Micro style for very small cells, otherwise SmallRegular
  if (h < 120 || w < 100) {
    tableFontSize_ = cat->ptSize(FontStyle::Micro);
  } else {
    tableFontSize_ = cat->ptSize(FontStyle::SmallRegular);
  }
}

SDL_Rect BandConditionsPanel::getActionRect(const std::string &action) const {
  (void)action; // No actions for this panel
  return {x_, y_, width_, height_};
}

nlohmann::json BandConditionsPanel::getDebugData() const {
  nlohmann::json data;

  if (!dataValid_) {
    data["status"] = "no_data";
    return data;
  }

  data["status"] = "valid";
  data["sfi"] = currentData_.sfi;
  data["k_index"] = currentData_.k_index;

  // Band conditions
  nlohmann::json bands = nlohmann::json::array();
  for (const auto &status : currentData_.statuses) {
    nlohmann::json band;
    band["band"] = status.band;
    band["day"] = stringForCondition(status.day);
    band["night"] = stringForCondition(status.night);
    bands.push_back(band);
  }
  data["bands"] = bands;

  return data;
}
