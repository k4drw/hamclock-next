#include "WeatherPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include <cstdio>

WeatherPanel::WeatherPanel(int x, int y, int w, int h, FontManager &fontMgr,
                           std::shared_ptr<WeatherStore> store,
                           const std::string &title)
    : Widget(x, y, w, h), fontMgr_(fontMgr), store_(std::move(store)),
      title_(title) {}

void WeatherPanel::update() {
  currentData_ = store_->get();
  dataValid_ = currentData_.valid;
}

static const char *degToDir(int deg) {
  static const char *dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  int idx = ((deg + 22) % 360) / 45;
  return dirs[idx];
}

void WeatherPanel::render(SDL_Renderer *renderer) {
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

  int centerX = x_ + width_ / 2;
  bool isNarrow = (width_ < 100);

  if (isNarrow) {
    // Vertical stack layout (Fidelity Mode style)
    int rowH = height_ / 4;
    auto drawNarrowRow = [&](const char *val, const char *lbl, int rowIdx,
                             SDL_Color valColor) {
      int ry = y_ + rowIdx * rowH;
      // Value (Large)
      fontMgr_.drawText(renderer, val, centerX, ry + rowH * 0.35f, valColor,
                        tempFontSize_, true, true);
      // Label (Small)
      fontMgr_.drawText(renderer, lbl, centerX, ry + rowH * 0.75f,
                        themes.textDim, infoFontSize_, false, true);
    };

    char valBuf[32];
    char lblBuf[32];
    const char *prefix = (title_.find("DE") != std::string::npos) ? "DE" : "DX";

    float temp =
        useMetric_ ? currentData_.temp : (currentData_.temp * 1.8f + 32.0f);
    float wind = useMetric_ ? currentData_.windSpeed
                            : (currentData_.windSpeed * 2.237f); // m/s to mph
    const char *tempUnit = useMetric_ ? "C" : "F";
    const char *windUnit = useMetric_ ? "m/s" : "mph";

    std::snprintf(valBuf, sizeof(valBuf), "%.1f", temp);
    std::snprintf(lblBuf, sizeof(lblBuf), "%s %s", prefix, tempUnit);
    drawNarrowRow(valBuf, lblBuf, 0, themes.success);

    std::snprintf(valBuf, sizeof(valBuf), "%d", currentData_.humidity);
    drawNarrowRow(valBuf, "Humidity", 1, themes.success);

    std::snprintf(valBuf, sizeof(valBuf), "%.0f", wind);
    std::snprintf(lblBuf, sizeof(lblBuf), "%s", windUnit);
    drawNarrowRow(valBuf, lblBuf, 2, themes.success);

    drawNarrowRow(degToDir(currentData_.windDeg), "Wind Dir", 3,
                  themes.success);
    return;
  }

  int pad = 8;
  int curY = y_ + pad;

  // Title
  fontMgr_.drawText(renderer, title_, centerX, curY, themes.accent,
                    labelFontSize_, true, true);
  curY += labelFontSize_ + 10;

  if (!dataValid_) {
    fontMgr_.drawText(renderer, "Waiting for data...", centerX,
                      y_ + height_ / 2, themes.textDim, infoFontSize_, false,
                      true);
    return;
  }

  // Temperature
  char buf[64];
  float temp =
      useMetric_ ? currentData_.temp : (currentData_.temp * 1.8f + 32.0f);
  const char *tempUnit = useMetric_ ? "C" : "F";
  std::snprintf(buf, sizeof(buf), "%.1f %s", temp, tempUnit);
  fontMgr_.drawText(renderer, buf, centerX, curY + tempFontSize_ / 2,
                    themes.text, tempFontSize_, true, true);
  curY += tempFontSize_ + 10;

  // Description
  fontMgr_.drawText(renderer, currentData_.description, centerX, curY,
                    themes.warning, infoFontSize_, false, true);
  curY += infoFontSize_ + 12;

  // Details Grid
  int colWidth = (width_ - 2 * pad) / 2;
  int detailY = curY;

  auto drawDetail = [&](const char *label, const char *value, int cx, int cy) {
    fontMgr_.drawText(renderer, label, cx, cy, themes.textDim,
                      infoFontSize_ - 2, false, true);
    fontMgr_.drawText(renderer, value, cx, cy + infoFontSize_, themes.text,
                      infoFontSize_, true, true);
  };

  // Humidity and Pressure
  std::snprintf(buf, sizeof(buf), "%d%%", currentData_.humidity);
  drawDetail("HUMID", buf, x_ + pad + colWidth / 2, detailY);

  std::snprintf(buf, sizeof(buf), "%.0f hPa", currentData_.pressure);
  drawDetail("PRESS", buf, x_ + width_ - pad - colWidth / 2, detailY);

  detailY += infoFontSize_ * 2 + 8;

  // Wind
  float wind =
      useMetric_ ? currentData_.windSpeed : (currentData_.windSpeed * 2.237f);
  const char *windUnit = useMetric_ ? "m/s" : "mph";
  std::snprintf(buf, sizeof(buf), "%.1f %s", wind, windUnit);
  drawDetail("WIND", buf, x_ + pad + colWidth / 2, detailY);

  std::snprintf(buf, sizeof(buf), "%d deg", currentData_.windDeg);
  drawDetail("DEG", buf, x_ + width_ - pad - colWidth / 2, detailY);
}

void WeatherPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::FastBold);
  tempFontSize_ = cat->ptSize(FontStyle::MediumBold);
  infoFontSize_ = cat->ptSize(FontStyle::Fast);

  if (w < 100) {
    tempFontSize_ = cat->ptSize(FontStyle::MediumBold); // Values
    infoFontSize_ = cat->ptSize(FontStyle::Micro);      // Labels
  } else if (h < 150) {
    tempFontSize_ = cat->ptSize(FontStyle::SmallBold);
  }
}
