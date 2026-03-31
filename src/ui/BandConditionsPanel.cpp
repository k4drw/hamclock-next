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

SDL_Color BandConditionsPanel::colorForCondition(BandCondition cond,
                                                  const ThemeColors &themes) {
  switch (cond) {
  case BandCondition::EXCELLENT:
    return themes.info;    // Cyan/info
  case BandCondition::GOOD:
    return themes.success; // Green
  case BandCondition::FAIR:
    return themes.warning; // Yellow
  case BandCondition::POOR:
    return themes.danger;  // Red
  default:
    return themes.textDim;
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

  renderChrome(renderer);

  int titleH = 20;
  auto *cat = fontMgr_.catalog();
  // Standard Title
  cat->drawText(renderer, "Band Conditions", x_ + 10, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  if (!dataValid_) {
    cat->drawText(renderer, "No Data", x_ + width_ / 2,
                  y_ + titleH + (height_ - titleH) / 2, themes.textDim,
                  FontStyle::Micro, true);
    return;
  }

  int pad = 4;

  int colWidth = (width_ - 2 * pad) / 3;
  int numRows =
      static_cast<int>(currentData_.statuses.size()) + 1; // +1 for header
  int availableH = height_ - titleH - 2 * pad;
  int rowHeight = availableH / numRows;

  // Use logical styles
  FontStyle cellStyle = (rowHeight < 15) ? FontStyle::Tiny : FontStyle::Micro;
  FontStyle headerStyle =
      (rowHeight < 15) ? FontStyle::TinyBold : FontStyle::MicroBold;

  SDL_Color headerColor = themes.accent;
  SDL_Color labelColor = themes.text;

  int curY = y_ + titleH + pad;

  auto drawInCol = [&](const std::string &text, int colIdx, SDL_Color color,
                       FontStyle style, int yOffset = 0) {
    int tx = x_ + pad + colIdx * colWidth + colWidth / 2;
    int ty = curY + rowHeight / 2 + yOffset;
    cat->drawText(renderer, text, tx, ty, color, style, true);
  };

  // Header
  bool useShort = (width_ < 100);
  if (!useShort) {
    drawInCol("Band", 0, headerColor, headerStyle, -2);
    drawInCol("Day", 1, headerColor, headerStyle, -2);
    drawInCol("Night", 2, headerColor, headerStyle, -2);
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
    drawInCol(status.band, 0, labelColor, cellStyle);
    drawInCol(stringForCondition(status.day, useShort), 1,
              colorForCondition(status.day, themes), cellStyle);
    drawInCol(stringForCondition(status.night, useShort), 2,
              colorForCondition(status.night, themes), cellStyle);
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

#include "WidgetRegistry.h"
REGISTER_WIDGET("band_conditions", "Band Cond", false, false, {
  return std::make_unique<BandConditionsPanel>(0, 0, 0, 0, deps.fontMgr, deps.bandStore);
})
