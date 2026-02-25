#include "EMEToolPanel.h"
#include "../core/Theme.h"
#include <cstdio>

EMEToolPanel::EMEToolPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<MoonStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(store) {}

void EMEToolPanel::update() { currentData_ = store_->get(); }

void EMEToolPanel::render(SDL_Renderer *renderer) {
  ThemeColors themes = getThemeColors(theme_);
  SDL_SetRenderDrawBlendMode(
      renderer, (theme_ == "glass") ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect rect = {x_, y_, width_, height_};
  SDL_RenderFillRect(renderer, &rect);

  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawRect(renderer, &rect);

  int titleH = 20;
  fontMgr_.drawText(renderer, "EME Planning Tool", x_ + 10, y_ + 5,
                    themes.accent, 10, true);

  int curY = y_ + titleH + 10;
  int centerX = x_ + width_ / 2;

  if (!currentData_.valid) {
    fontMgr_.drawText(renderer, "Calculating...", centerX,
                      y_ + titleH + (height_ - titleH) / 2, themes.textDim, 10,
                      false, true);
    return;
  }

  auto drawRow = [&](const char *label, const char *value, SDL_Color valCol) {
    int fontSize = 9;
    fontMgr_.drawText(renderer, label, x_ + 10, curY, themes.text, fontSize);
    int valW = fontMgr_.getLogicalWidth(value, fontSize);
    int valX = x_ + width_ - 10 - valW;
    fontMgr_.drawText(renderer, value, valX, curY, valCol, fontSize);
    curY += 18;
  };

  char buf[64];

  std::snprintf(buf, sizeof(buf), "%.1f deg", currentData_.elevation);
  drawRow("DE Elev:", buf,
          (currentData_.elevation > 0) ? SDL_Color{0, 255, 0, 255}
                                       : themes.textDim);

  std::snprintf(buf, sizeof(buf), "%.1f deg", currentData_.dx_elevation);
  drawRow("DX Elev:", buf,
          (currentData_.dx_elevation > 0) ? SDL_Color{0, 255, 0, 255}
                                          : themes.textDim);

  SDL_Color windowCol = currentData_.mutual_window
                            ? SDL_Color{0, 255, 100, 255}
                            : SDL_Color{255, 100, 100, 255};
  drawRow("Mutual Window:", currentData_.mutual_window ? "OPEN" : "CLOSED",
          windowCol);

  curY += 10;
  std::snprintf(buf, sizeof(buf), "%.1f dB", currentData_.path_loss_db);
  drawRow("Path Loss (144):", buf, themes.accent);

  double dist = currentData_.distance_km;
  const char *unit = "km";
  if (!useMetric_) {
    dist *= 0.621371;
    unit = "mi";
  }
  std::snprintf(buf, sizeof(buf), "%.0f %s", dist, unit);
  drawRow("Distance:", buf, themes.text);
}

nlohmann::json EMEToolPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  if (!currentData_.valid)
    return j;
  j["de_elevation"] = currentData_.elevation;
  j["dx_elevation"] = currentData_.dx_elevation;
  j["mutual_window"] = currentData_.mutual_window;
  j["path_loss_db"] = currentData_.path_loss_db;
  j["distance_km"] = currentData_.distance_km;
  return j;
}
