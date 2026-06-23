#include "LaunchPanel.h"
#include "WidgetRegistry.h"
#include "FontCatalog.h"
#include "../core/Theme.h"
#include "RenderUtils.h"
#include <fmt/core.h>
#include <ctime>

LaunchPanel::LaunchPanel(int x, int y, int w, int h, FontManager &fontMgr, LaunchProvider *launchProvider)
    : Widget(x, y, w, h), fontMgr_(fontMgr), launchProvider_(launchProvider) {
}

void LaunchPanel::update() {
  if (launchProvider_) {
    auto allLaunches = launchProvider_->getUpcoming();
    upcomingLaunches_.clear();
    uint32_t nowT = std::time(nullptr);
    for (const auto& ev : allLaunches) {
      if (static_cast<int>(ev.windowStart - nowT) >= 0) {
        upcomingLaunches_.push_back(ev);
      }
    }
  }
}

void LaunchPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  if (upcomingLaunches_.empty()) {
    cat->drawText(renderer, "Awaiting Launches...", x_ + 10, y_ + height_ / 2 - 8, themes.textDim, FontStyle::SmallRegular);
    return;
  }

  uint32_t nowT = std::time(nullptr);
  int ry = y_ + 5;
  int rowHeight = 22;

  // Draw header
  cat->drawText(renderer, "Upcoming Launches", x_ + 10, ry, themes.accent, FontStyle::MicroBold);
  ry += rowHeight;

  // Draw launches
  for (const auto& ev : upcomingLaunches_) {
    if (ry + rowHeight > y_ + height_ - 5) break;

    int seconds = ev.windowStart - nowT;
    std::string tminus = fmt::format("T-{:02d}:{:02d}:{:02d}", seconds / 3600, (seconds % 3600) / 60, seconds % 60);

    // Rocket icon simulation
    RenderUtils::drawTriangle(renderer, x_ + 10, ry + 12, x_ + 16, ry + 12, x_ + 13, ry + 6, themes.text);
    RenderUtils::drawRect(renderer, x_ + 11, ry + 12, 4, 4, themes.text);

    // Clip mission name to avoid overlapping T-minus
    SDL_Rect clipRect = {x_ + 22, ry, width_ - 22 - 75, rowHeight};
    SDL_RenderSetClipRect(renderer, &clipRect);
    cat->drawText(renderer, ev.missionName, x_ + 22, ry + 4, themes.text, FontStyle::Micro);
    SDL_RenderSetClipRect(renderer, nullptr);
    
    SDL_Color tColor = (seconds < 3600) ? themes.warning : themes.textDim;
    cat->drawText(renderer, tminus, x_ + width_ - 10, ry + 4, tColor, FontStyle::Micro, false, true);

    ry += rowHeight;
  }

  renderTooltip(renderer, fontMgr_);
}

void LaunchPanel::onMouseMove(int mx, int my) {
  // Hide tooltip if maximized (width > 400px typically means maximized)
  if (upcomingLaunches_.empty() || width_ > 400) {
    tooltip_.text.clear();
    return;
  }
  
  int headerH = 22;
  int ry = y_ + 5 + headerH;
  int rowHeight = 22;

  if (mx < x_ || mx > x_ + width_ || my < ry || my > y_ + height_ - 5) {
    tooltip_.text.clear();
    return;
  }

  int idx = (my - ry) / rowHeight;
  if (idx >= 0 && idx < static_cast<int>(upcomingLaunches_.size())) {
    const auto& ev = upcomingLaunches_[idx];
    tooltip_.text = ev.missionName;
    tooltip_.text2 = ev.providerName + " | " + ev.padName;
    tooltip_.x = mx;
    tooltip_.y = my;
  } else {
    tooltip_.text.clear();
  }
}

REGISTER_WIDGET("rocket_launches", "Rocket Launches", true, false, {
  return std::make_unique<LaunchPanel>(0, 0, 0, 0, deps.fontMgr, deps.launchProvider);
})
