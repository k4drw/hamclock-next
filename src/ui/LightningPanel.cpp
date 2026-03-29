#include "LightningPanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"

namespace HamClock {

LightningPanel::LightningPanel(int x, int y, int w, int h, FontManager &fontMgr)
    : Widget(x, y, w, h), fontMgr_(fontMgr) {}

void LightningPanel::updateData(const LightningData &data) {
  std::lock_guard<std::mutex> lock(mutex_);
  data_ = data;
}

void LightningPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready()) return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();

  renderChrome(renderer);

  cat->drawText(renderer, "QRN / Lightning", x_ + 10, y_ + 5, themes.accent, FontStyle::MicroBold);

  LightningData d;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    d = data_;
  }

  if (!d.valid) {
    cat->drawText(renderer, "Waiting for data...", x_ + width_/2, y_ + height_/2, themes.textDim, FontStyle::Fast, true);
    return;
  }

  int curY = y_ + 25;
  int pad = 10;

  // SAFETY ALERT
  if (d.safetyAlert) {
    SDL_Rect alertBox = {x_ + 5, curY, width_ - 10, 25};
    Uint32 ms = SDL_GetTicks();
    if ((ms / 500) % 2 == 0) { // Blink
      SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, themes.danger.a);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.danger.r, themes.danger.g, themes.danger.b, 128);
    }
    SDL_RenderFillRect(renderer, &alertBox);
    cat->drawText(renderer, "STRIKE NEARBY", x_ + width_/2, curY + 12, themes.bg, FontStyle::MicroBold, true);
    curY += 30;
  }

  // Count
  char buf[64];
  std::snprintf(buf, sizeof(buf), "10m: %d strikes", d.strikesLast10Min);
  cat->drawText(renderer, buf, x_ + pad, curY, themes.text, FontStyle::Fast);
  curY += 20;

  std::snprintf(buf, sizeof(buf), "30m: %d strikes", d.strikesLast30Min);
  cat->drawText(renderer, buf, x_ + pad, curY, themes.textDim, FontStyle::Micro);
  curY += 20;

  // Closest
  if (d.closestStrikeKm >= 0) {
    const char* dir = "N";
    if (d.closestBearing > 22.5) dir = "NE";
    if (d.closestBearing > 67.5) dir = "E";
    if (d.closestBearing > 112.5) dir = "SE";
    if (d.closestBearing > 157.5) dir = "S";
    if (d.closestBearing > 202.5) dir = "SW";
    if (d.closestBearing > 247.5) dir = "W";
    if (d.closestBearing > 292.5) dir = "NW";
    if (d.closestBearing > 337.5) dir = "N";

    std::snprintf(buf, sizeof(buf), "Closest: %.1f km (%s)", d.closestStrikeKm, dir);
    cat->drawText(renderer, buf, x_ + pad, curY, themes.accent, FontStyle::Fast);
  }
}

} // namespace HamClock

REGISTER_WIDGET("lightning", "Lightning", false, false, {
  return std::make_unique<HamClock::LightningPanel>(0, 0, 0, 0, deps.fontMgr);
})
