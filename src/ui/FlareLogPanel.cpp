#include "FlareLogPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "WidgetRegistry.h"
#include <SDL.h>
#include <cstdio>

FlareLogPanel::FlareLogPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             std::shared_ptr<FlareDataStore> flareStore)
    : ListPanel(x, y, w, h, fontMgr, "Solar Flares", {}),
      flareStore_(std::move(flareStore)) {}

FlareLogPanel::~FlareLogPanel() {}

void FlareLogPanel::update() {}

void FlareLogPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
}

void FlareLogPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  if (!fontMgr_.ready())
    return;

  // Render the title bar (from ListPanel)
  renderChrome(renderer);
  renderTitle(renderer, fontMgr_, getDisplayName());

  if (!flareStore_) {
    cat->drawText(renderer, "No store", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  FlareData data = flareStore_->get();
  if (!data.valid || data.events.empty()) {
    cat->drawText(renderer, "No flares", x_ + width_ / 2, y_ + height_ / 2,
                  themes.textDim, FontStyle::Micro, true, false, true);
    return;
  }

  // Render flare list
  const int row_h = 14;
  const int start_y = y_ + 20;
  const int max_rows = (height_ - 24) / row_h;

  for (int i = 0; i < (int)data.events.size() && i < max_rows; i++) {
    const auto &ev = data.events[i];
    int row_y = start_y + i * row_h;

    // Color by class: gray=B/C, yellow=M, red=X
    SDL_Color color = themes.textDim;
    if (ev.class_type == "X") {
      color = {220, 60, 60, 255};    // red
    } else if (ev.class_type == "M") {
      color = {240, 220, 30, 255};   // yellow
    } else if (ev.class_type == "B" || ev.class_type == "C") {
      color = {100, 100, 100, 255};  // gray
    }

    // Format: "[HH:MM] Class (begin–end)"
    char buf[64];
    std::snprintf(buf, sizeof(buf), "[%s] %s (%s)", ev.peakTimeString().c_str(),
                  ev.class_type.c_str(), ev.durationString().c_str());

    cat->drawText(renderer, buf, x_ + 6, row_y, color, FontStyle::Tiny);
  }
}

REGISTER_WIDGET("flare_log", "Solar Flares", false, false, {
  return std::make_unique<FlareLogPanel>(0, 0, 0, 0, deps.fontMgr,
                                         deps.flareStore);
})
