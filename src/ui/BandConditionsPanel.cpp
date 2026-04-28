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
  renderTitle(renderer, fontMgr_, "Band Conditions");

  if (!dataValid_) {
    auto *cat = fontMgr_.catalog();
    cat->drawText(renderer, "No Data", x_ + width_ / 2,
                  y_ + height_ / 2, themes.textDim,
                  FontStyle::Micro, true);
    return;
  }

  auto *cat = fontMgr_.catalog();
  const int row_h = 16;
  const int col1_w = 30;
  const int start_y = y_ + 20;

  // Header
  cat->drawText(renderer, "Band", x_ + 6, start_y, themes.accent, FontStyle::Tiny);
  cat->drawText(renderer, "Day", x_ + 6 + col1_w, start_y, themes.accent, FontStyle::Tiny);
  cat->drawText(renderer, "Night", x_ + 6 + col1_w + 40, start_y, themes.accent, FontStyle::Tiny);

  // Rows
  for (size_t i = 0; i < currentData_.statuses.size(); i++) {
    int row_y = start_y + (i + 1) * row_h;
    const auto &status = currentData_.statuses[i];

    cat->drawText(renderer, status.band, x_ + 6, row_y, themes.text, FontStyle::Tiny);
    cat->drawText(renderer, stringForCondition(status.day), x_ + 6 + col1_w, row_y,
                  colorForCondition(status.day, themes), FontStyle::Tiny);
    cat->drawText(renderer, stringForCondition(status.night), x_ + 6 + col1_w + 40, row_y,
                  colorForCondition(status.night, themes), FontStyle::Tiny);
  }
}

void BandConditionsPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
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
