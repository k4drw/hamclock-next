#include "SolarCyclePanel.h"
#include "WidgetRegistry.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "GraphHelper.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <vector>

SolarCyclePanel::SolarCyclePanel(int x, int y, int w, int h,
                                 FontManager &fontMgr, TextureManager &texMgr,
                                 std::shared_ptr<HistoryStore> store)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      store_(std::move(store)) {}

void SolarCyclePanel::update() { currentSSN_ = store_->get("ssn"); }

void SolarCyclePanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);
  auto *cat = fontMgr_.catalog();
  if (!cat)
    return;

  renderChrome(renderer);

  const int pad = 8;
  cat->drawText(renderer, "Solar Cycle 25", x_ + pad, y_ + 5, themes.accent,
                FontStyle::MicroBold);

  // Compute cycle age
  std::time_t now_t = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  long ageSeconds = static_cast<long>(now_t - kSC25StartTs);
  int ageMonths = static_cast<int>(ageSeconds / (365.25 / 12.0 * 86400.0));
  int ageYears = ageMonths / 12;
  int remMonths = ageMonths % 12;

  // Phase classification
  const char *phase;
  SDL_Color phaseColor;
  if (ageMonths < 30) {
    phase = "Rising";
    phaseColor = themes.success;
  } else if (ageMonths < 48) {
    phase = "Near Max";
    phaseColor = themes.warning;
  } else if (ageMonths < 72) {
    phase = "Declining";
    phaseColor = themes.info;
  } else {
    phase = "Late Cycle";
    phaseColor = themes.textDim;
  }

  int titleH = 22;
  int statsH = 38;
  int graphX = x_ + pad;
  int graphY = y_ + titleH;
  int graphW = width_ - 2 * pad;
  int graphH = height_ - titleH - statsH - pad;

  if (graphH < 20) {
    // Panel too small for chart — stats only
    char buf[48];
    std::snprintf(buf, sizeof(buf), "SC25 %dy%dm | %s", ageYears, remMonths,
                  phase);
    cat->drawText(renderer, buf, x_ + width_ / 2, y_ + height_ / 2,
                  phaseColor, FontStyle::Fast, true, false, true);
    return;
  }

  // Axes
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 255);
  SDL_RenderDrawLine(renderer, graphX, graphY, graphX, graphY + graphH);
  SDL_RenderDrawLine(renderer, graphX, graphY + graphH, graphX + graphW,
                     graphY + graphH);

  if (currentSSN_.valid && !currentSSN_.points.empty()) {
    int n = static_cast<int>(currentSSN_.points.size());
    float minV = std::max(0.0f, currentSSN_.minValue);
    float maxV = std::max(minV + 1.0f, currentSSN_.maxValue);
    float range = maxV - minV;
    float stepX = (float)graphW / std::max(1, n - 1);

    SDL_Texture *lineTex = texMgr_.get("line_aa");
    std::vector<SDL_FPoint> pts;
    pts.reserve(n);
    for (int i = 0; i < n; ++i) {
      float val = currentSSN_.points[i].value;
      float py = graphY + graphH - ((val - minV) / range) * (float)graphH;
      pts.push_back({graphX + i * stepX, py});
    }
    GraphHelper::drawPolyline(renderer, lineTex, pts.data(), n, themes.accent);

    // Current SSN large overlay
    float latestSSN = currentSSN_.points.back().value;
    char ssnBuf[16];
    std::snprintf(ssnBuf, sizeof(ssnBuf), "%.0f", latestSSN);
    cat->drawText(renderer, ssnBuf, graphX + graphW / 2, graphY + 14,
                  themes.text, FontStyle::MediumBold, true, false, true);
    cat->drawText(renderer, "SSN", graphX + graphW / 2, graphY + 34,
                  themes.textDim, FontStyle::Tiny, true);
  } else {
    cat->drawText(renderer, "No SSN Data", x_ + width_ / 2,
                  graphY + graphH / 2, themes.textDim, FontStyle::Fast, true,
                  false, true);
  }

  // Stats row
  int statsY = y_ + height_ - statsH;
  char ageBuf[32];
  std::snprintf(ageBuf, sizeof(ageBuf), "Age: %dy %dm", ageYears, remMonths);
  cat->drawText(renderer, ageBuf, x_ + pad, statsY, themes.text,
                FontStyle::Fast);
  cat->drawText(renderer, phase, x_ + width_ - pad, statsY, phaseColor,
                FontStyle::Fast, false, true);

  // Days since/to predicted max
  long secToPred = static_cast<long>(kSC25PredMaxTs - now_t);
  char maxBuf[48];
  if (secToPred > 0) {
    int daysTo = static_cast<int>(secToPred / 86400);
    std::snprintf(maxBuf, sizeof(maxBuf), "Pred. max in ~%d days", daysTo);
  } else {
    int daysPast = static_cast<int>(-secToPred / 86400);
    std::snprintf(maxBuf, sizeof(maxBuf), "Past pred. max %d days", daysPast);
  }
  cat->drawText(renderer, maxBuf, x_ + pad, statsY + 18, themes.textDim,
                FontStyle::Tiny);
}

void SolarCyclePanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
}

REGISTER_WIDGET("solar_cycle", "Solar Cycle", false, false, {
  return std::make_unique<SolarCyclePanel>(0, 0, 0, 0, deps.fontMgr, deps.texMgr, deps.historyStore);
})
