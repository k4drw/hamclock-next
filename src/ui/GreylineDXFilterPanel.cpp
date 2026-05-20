#include "GreylineDXFilterPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"
#include <SDL.h>
#include <cstdio>
#include <ctime>
#include <cmath>

GreylineDXFilterPanel::GreylineDXFilterPanel(
    int x, int y, int w, int h, FontManager &fontMgr,
    std::shared_ptr<DXClusterDataStore> dxcStore)
    : ListPanel(x, y, w, h, fontMgr, "Greyline DX", {}),
      dxcStore_(std::move(dxcStore)) {}

GreylineDXFilterPanel::~GreylineDXFilterPanel() {}

void GreylineDXFilterPanel::update() {}

void GreylineDXFilterPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
}

bool GreylineDXFilterPanel::isNearGreyline(double lat) const {
  // Simple heuristic: sun altitude within ±6 degrees (civil twilight)
  // At latitude lat, civil twilight occurs roughly between 6am-6pm local solar time
  // Simplified: return true if between 5:30am and 6:30pm UTC-adjusted
  time_t now = time(nullptr);
  struct tm *tm_now = localtime(&now);

  int hour = tm_now->tm_hour;
  int min = tm_now->tm_min;
  int hour_frac = hour * 60 + min;

  // 5:30am = 330 min, 6:30pm = 1230 min
  // Wider window since we don't have precise solar calc
  return hour_frac >= 330 && hour_frac <= 1230;
}

void GreylineDXFilterPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  if (!fontMgr_.ready())
    return;

  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, getDisplayName());

  if (!dxcStore_) {
    cat->drawText(renderer, "No store", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  auto data_snapshot = dxcStore_->snapshot();
  if (!data_snapshot)
    return;
  const DXClusterData &data = *data_snapshot;
  if (data.spots.empty()) {
    cat->drawText(renderer, "No spots", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  // Filter and render spots near greyline
  const int row_h = 14;
  const int start_y = y_ + 20;
  const int max_rows = (height_ - 24) / row_h;

  int row_count = 0;
  for (const auto &spot : data.spots) {
    // Check if TX or RX is near greyline
    if (!isNearGreyline(spot.txLat) && !isNearGreyline(spot.rxLat))
      continue;

    if (row_count >= max_rows)
      break;

    int row_y = start_y + row_count * row_h;

    // Format: "callsign → callsign freq"
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%s → %s %.0fKHz", spot.txCall.c_str(),
                  spot.rxCall.c_str(), spot.freqKhz);

    cat->drawText(renderer, buf, x_ + 6, row_y, themes.text, FontStyle::Tiny);

    row_count++;
  }

  if (row_count == 0) {
    cat->drawText(renderer, "None on greyline", x_ + width_ / 2,
                  y_ + height_ / 2, themes.textDim, FontStyle::Tiny, true);
  }
}

REGISTER_WIDGET("greyline_spots", "Greyline Spots", false, false, {
  return std::make_unique<GreylineDXFilterPanel>(0, 0, 0, 0, deps.fontMgr,
                                                 deps.dxcStore);
})
