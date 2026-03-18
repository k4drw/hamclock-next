#include "SpaceWeatherPanel.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "GraphHelper.h"
#include "RenderUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

SpaceWeatherPanel::SpaceWeatherPanel(
    int x, int y, int w, int h, FontManager &fontMgr, TextureManager &texMgr,
    std::shared_ptr<SolarDataStore> store,
    std::shared_ptr<XRayHistoryStore> xrayStore)
    : Widget(x, y, w, h), fontMgr_(fontMgr), texMgr_(texMgr),
      store_(std::move(store)), xrayStore_(std::move(xrayStore)) {
  items_[0].label = "SFI";
  items_[1].label = "SN";
  items_[2].label = "A";
  items_[3].label = "K";
  items_[4].label = "Wind";
  items_[5].label = "Dens";
  items_[6].label = "Bz";
  items_[7].label = "Bt";
  items_[8].label = "DST";
  items_[9].label = "Aurora";
  items_[10].label = "DRAP";
  items_[11].label = "-";
  items_[12].label = "R-Scale";
  items_[13].label = "S-Scale";
  items_[14].label = "G-Scale";
}

SDL_Color SpaceWeatherPanel::colorForK(float k, const ThemeColors &themes) {
  if (k < 3)
    return themes.success; // Green
  if (k <= 4)
    return themes.warning; // Yellow
  return themes.danger;    // Red
}

SDL_Color SpaceWeatherPanel::colorForSFI(int sfi, const ThemeColors &themes) {
  if (sfi > 100)
    return themes.success; // Green
  if (sfi > 70)
    return themes.warning; // Yellow
  return themes.danger;    // Red
}

SDL_Color SpaceWeatherPanel::colorForNOAAScale(int scale,
                                               const ThemeColors &themes) {
  // Official NOAA 5-tier scale:
  // 0 = no event, 1 = Minor, 2 = Moderate, 3 = Strong, 4/5 = Severe/Extreme
  if (scale <= 0)
    return themes.textDim; // No event — muted
  if (scale == 1)
    return themes.success; // Minor — green
  if (scale == 2)
    return themes.warning; // Moderate — yellow
  if (scale == 3)
    return themes.warning; // Strong — yellow (fallback to theme warning)
  return themes.danger;        // Severe/Extreme — red
}

void SpaceWeatherPanel::destroyCache() {
  for (auto &item : items_) {
    if (item.labelTex) {
      MemoryMonitor::getInstance().destroyTexture(item.labelTex);
    }
    if (item.valueTex) {
      MemoryMonitor::getInstance().destroyTexture(item.valueTex);
    }
    item.lastValue.clear();
    item.lastValueColor = {0, 0, 0, 0};
  }
}

void SpaceWeatherPanel::update() {
  ThemeColors themes = getThemeColors(theme_);
  if (xrayStore_)
    sparklineHistory_ = xrayStore_->getHistory();

  SolarData data = store_->get();
  dataValid_ = data.valid;
  if (!data.valid)
    return;

  char buf[16];

  std::snprintf(buf, sizeof(buf), "%d", data.sfi);
  items_[0].value = buf;

  std::snprintf(buf, sizeof(buf), "%d", data.sunspot_number);
  items_[1].value = buf;
  items_[1].valueColor = themes.success;

  std::snprintf(buf, sizeof(buf), "%d", data.a_index);
  items_[2].value = buf;
  items_[2].valueColor = {255, 255, 255, 255};

  std::snprintf(buf, sizeof(buf), "%.1f", data.k_index);
  items_[3].value = buf;

  // Solar wind speed: km/s (metric) or mph (imperial)
  // 1 km/s = 2236.94 mph;  previous code used 0.621371 (km→mi) which was wrong
  float windSpd =
      useMetric_ ? data.solar_wind_speed : (data.solar_wind_speed * 2236.94f);
  std::snprintf(buf, sizeof(buf), "%.0f %s", windSpd,
                useMetric_ ? "km/s" : "mph");
  items_[4].value = buf;
  items_[4].valueColor = themes.warning;

  items_[0].valueColor = colorForSFI(data.sfi, themes);
  items_[3].valueColor = colorForK(data.k_index, themes);

  std::snprintf(buf, sizeof(buf), "%.1f", data.solar_wind_density);
  items_[5].value = buf;
  items_[5].valueColor = themes.info;

  std::snprintf(buf, sizeof(buf), "%d", data.bz);
  items_[6].value = buf;
  items_[6].valueColor = (data.bz < 0) ? themes.danger : themes.success;

  std::snprintf(buf, sizeof(buf), "%d", data.bt);
  items_[7].value = buf;
  items_[7].valueColor = themes.text;

  std::snprintf(buf, sizeof(buf), "%d", data.dst);
  items_[8].value = buf;
  items_[8].valueColor = (data.dst < -50) ? themes.danger : themes.text;

  std::snprintf(buf, sizeof(buf), "%d", data.aurora);
  items_[9].value = buf;
  items_[9].valueColor = (data.aurora > 50) ? themes.warning : themes.info;

  std::snprintf(buf, sizeof(buf), "%d", data.drap);
  items_[10].value = buf;
  items_[10].valueColor = themes.info;

  items_[11].value = "-";

  // NOAA R-Scale (Radio Blackouts)
  std::snprintf(buf, sizeof(buf), "R%d", data.noaa_r_scale);
  items_[12].value = buf;
  items_[12].valueColor = colorForNOAAScale(data.noaa_r_scale, themes);

  // NOAA S-Scale (Solar Radiation Storms)
  std::snprintf(buf, sizeof(buf), "S%d", data.noaa_s_scale);
  items_[13].value = buf;
  items_[13].valueColor = colorForNOAAScale(data.noaa_s_scale, themes);

  // NOAA G-Scale (Geomagnetic Storms)
  std::snprintf(buf, sizeof(buf), "G%d", data.noaa_g_scale);
  items_[14].value = buf;
  items_[14].valueColor = colorForNOAAScale(data.noaa_g_scale, themes);

  // Alert badge: activate when any scale >= 3 (Strong or above).
  alertBadge_.active = false;
  if (data.noaa_g_scale >= 3) {
    alertBadge_.active = true;
    std::snprintf(buf, sizeof(buf), "G%d!", data.noaa_g_scale);
    alertBadge_.text = buf;
    alertBadge_.color = colorForNOAAScale(data.noaa_g_scale, themes);
  } else if (data.noaa_r_scale >= 3) {
    alertBadge_.active = true;
    std::snprintf(buf, sizeof(buf), "R%d!", data.noaa_r_scale);
    alertBadge_.text = buf;
    alertBadge_.color = colorForNOAAScale(data.noaa_r_scale, themes);
  } else if (data.noaa_s_scale >= 3) {
    alertBadge_.active = true;
    std::snprintf(buf, sizeof(buf), "S%d!", data.noaa_s_scale);
    alertBadge_.text = buf;
    alertBadge_.color = colorForNOAAScale(data.noaa_s_scale, themes);
  }
}

void SpaceWeatherPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready())
    return;

  ThemeColors themes = getThemeColors(theme_);

  // Cycle pages every 7 seconds
  uint32_t now = SDL_GetTicks();
  if (now - lastPageUpdate_ > 7000) {
    currentPage_ = (currentPage_ + 1) % 4;
    lastPageUpdate_ = now;
    destroyCache(); // Force redraw of new page items
  }

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

  int titleH = 20;
  bool isSmall = (width_ < 150);
  const char *titleText = isSmall ? "Space WX" : "Space Weather";
  FontStyle titleStyle =
      isSmall ? FontStyle::CaptionBold : FontStyle::MicroBold;
  fontMgr_.catalog()->drawText(renderer, titleText, x_ + 10, y_ + 5,
                               themes.accent, titleStyle);

  if (!dataValid_) {
    fontMgr_.catalog()->drawText(renderer, "Awaiting data...", x_ + 10,
                                 y_ + titleH + (height_ - titleH) / 2 - 8,
                                 themes.textDim, FontStyle::Micro);
    return;
  }

  bool labelFontChanged = (labelFontSize_ != lastLabelFontSize_);
  bool valueFontChanged = (valueFontSize_ != lastValueFontSize_);

  // Grid layout for current page
  bool isNarrow = (width_ < 110);
  int numCols = isNarrow ? 1 : 2;
  int numRows = isNarrow ? 4 : 2;

  bool showSparkline =
      (!isNarrow && xrayStore_ && !sparklineHistory_.empty() && height_ >= 80);
  int sparklineH = showSparkline ? 22 : 0;

  int cellW = width_ / numCols;
  int cellH = (height_ - titleH - sparklineH - 10) /
              numRows; // Leave space for title, sparkline, and pagination bar
  int pad = std::max(1, static_cast<int>(cellH * 0.05f));

  SDL_Color labelColor = themes.textDim;

  int startIdx = currentPage_ * kItemsPerPage;
  for (int i = 0; i < kItemsPerPage; ++i) {
    int itemIdx = startIdx + i;

    // Bounds check - skip if we've exceeded the number of items
    if (itemIdx >= kNumItems)
      continue;

    int col = i % numCols;
    int row = i / numCols;
    int cellX = x_ + col * cellW;
    int cellY = y_ + titleH + row * cellH;

    // Label (cached until font size changes)
    if (labelFontChanged || !items_[itemIdx].labelTex) {
      if (items_[itemIdx].labelTex) {
        MemoryMonitor::getInstance().destroyTexture(items_[itemIdx].labelTex);
      }
      items_[itemIdx].labelTex = fontMgr_.renderText(
          renderer, items_[itemIdx].label, labelColor, labelFontSize_,
          &items_[itemIdx].labelW, &items_[itemIdx].labelH, false);
    }

    // Value (re-render on data or font change, or color change)
    bool colorChanged =
        std::memcmp(&items_[itemIdx].valueColor,
                    &items_[itemIdx].lastValueColor, sizeof(SDL_Color)) != 0;
    if (items_[itemIdx].value != items_[itemIdx].lastValue ||
        valueFontChanged || colorChanged) {
      if (items_[itemIdx].valueTex) {
        MemoryMonitor::getInstance().destroyTexture(items_[itemIdx].valueTex);
      }
      int vSize = valueFontSize_;
      // Special case: Wind text in Pane 4 (isSmall) often needs shrinking
      if (isSmall && itemIdx == 4) {
        vSize = std::max(8, vSize - 2);
      }

      // Initial render
      items_[itemIdx].valueTex = fontMgr_.renderText(
          renderer, items_[itemIdx].value, items_[itemIdx].valueColor, vSize,
          &items_[itemIdx].valueW, &items_[itemIdx].valueH, true);

      // --- Shrink-to-fit logic ---
      // If width exceeds cell (minus padding), re-render smaller
      int maxW = cellW - 4;
      while (items_[itemIdx].valueW > maxW && vSize > 8) {
        MemoryMonitor::getInstance().destroyTexture(items_[itemIdx].valueTex);
        vSize--;
        items_[itemIdx].valueTex = fontMgr_.renderText(
            renderer, items_[itemIdx].value, items_[itemIdx].valueColor, vSize,
            &items_[itemIdx].valueW, &items_[itemIdx].valueH, true);
      }

      items_[itemIdx].lastValue = items_[itemIdx].value;
      items_[itemIdx].lastValueColor = items_[itemIdx].valueColor;
    }

    // Draw label (top of cell, centered)
    if (items_[itemIdx].labelTex) {
      int lx = cellX + (cellW - items_[itemIdx].labelW) / 2;
      int ly = cellY + pad;
      SDL_Rect dst = {lx, ly, items_[itemIdx].labelW, items_[itemIdx].labelH};
      SDL_RenderCopy(renderer, items_[itemIdx].labelTex, nullptr, &dst);
    }

    // Draw value (below label, centered)
    if (items_[itemIdx].valueTex) {
      int vx = cellX + (cellW - items_[itemIdx].valueW) / 2;
      // Use a slight overlap prevention or specific offset
      int vy = cellY + cellH - pad - items_[itemIdx].valueH -
               2; // Offset from bottom of cell
      if (vy < cellY + items_[itemIdx].labelH + pad) {
        vy = cellY + items_[itemIdx].labelH + pad - 1; // Minimal spacing
      }
      SDL_Rect dst = {vx, vy, items_[itemIdx].valueW, items_[itemIdx].valueH};
      SDL_RenderCopy(renderer, items_[itemIdx].valueTex, nullptr, &dst);
    }
  }

  // Draw X-ray flux sparkline (above pagination bar)
  if (showSparkline) {
    const int pad = 4;
    const int slX = x_ + pad;
    const int slW = width_ - 2 * pad;
    const int slY = y_ + titleH + numRows * cellH + 2;
    const int slH = sparklineH - 4;

    // Log-scale Y mapping: 1e-8 (bottom) to 1e-3 (top)
    const double logMin = -8.0; // log10(1e-8)
    const double logMax = -3.0; // log10(1e-3)
    auto fluxToY = [&](double flux) -> int {
      double logVal = std::log10(std::max(flux, 1e-9));
      double t = (logVal - logMin) / (logMax - logMin);
      t = std::max(0.0, std::min(1.0, t));
      return slY + slH - static_cast<int>(t * slH);
    };

    // Threshold lines (A=1e-8, B=1e-7, C=1e-6, M=1e-5, X=1e-4)
    static const double kThresholds[] = {1e-8, 1e-7, 1e-6, 1e-5, 1e-4};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (double thr : kThresholds) {
      int ty = fluxToY(thr);
      if (ty < slY || ty > slY + slH)
        continue;
      bool isX = (thr >= 1e-4);
      SDL_SetRenderDrawColor(renderer, isX ? 255 : 80, isX ? 140 : 80,
                             isX ? 0 : 80, 160);
      SDL_RenderDrawLine(renderer, slX, ty, slX + slW, ty);
    }

    // Time-range: last 6 hours
    auto now = std::chrono::system_clock::now();
    auto tMin = now - std::chrono::hours(6);
    double tSpan = std::chrono::duration<double>(now - tMin).count();

    auto timeToX = [&](std::chrono::system_clock::time_point tp) -> int {
      double age = std::chrono::duration<double>(tp - tMin).count();
      return slX + static_cast<int>(age / tSpan * slW);
    };

    // Draw sparkline (AA polyline)
    std::vector<SDL_FPoint> slPts;
    slPts.reserve(sparklineHistory_.size());
    for (const auto &pt : sparklineHistory_) {
      if (pt.timestamp < tMin)
        continue;
      slPts.push_back({static_cast<float>(timeToX(pt.timestamp)),
                       static_cast<float>(fluxToY(pt.flux))});
    }
    GraphHelper::drawPolyline(renderer, texMgr_.get("line_aa"), slPts.data(),
                              static_cast<int>(slPts.size()), themes.info, 1.5f);
  }

  // Draw pagination bar (bottom) - 4 segments
  int barH = 2;
  int segPad = 2;
  int barY = y_ + height_ - barH - 2;
  int totalBarW = width_ - 2 * segPad;
  int segW = totalBarW / 4;

  for (int i = 0; i < 4; ++i) {
    SDL_Rect seg = {x_ + segPad + i * segW, barY, segW - 1, barH};
    if (i == currentPage_) {
      SDL_SetRenderDrawColor(renderer, themes.accent.r, themes.accent.g,
                             themes.accent.b, themes.accent.a);
    } else {
      SDL_SetRenderDrawColor(renderer, themes.textDim.r, themes.textDim.g,
                             themes.textDim.b, themes.textDim.a);
    }
    SDL_RenderFillRect(renderer, &seg);
  }

  // Alert badge (top-right corner): flash at 1 Hz when active.
  if (alertBadge_.active) {
    uint32_t ticks = SDL_GetTicks();
    bool visible = (ticks / 500) % 2 == 0; // Blink every 500 ms
    if (visible) {
      fontMgr_.catalog()->drawText(renderer, alertBadge_.text, x_ + width_ - 4,
                                   y_ + 3, alertBadge_.color,
                                   FontStyle::MicroBold, true);
    }
  }

  if (tooltip_.visible) {
    renderTooltip(renderer, fontMgr_);
  }

  lastLabelFontSize_ = labelFontSize_;
  lastValueFontSize_ = valueFontSize_;
}

void SpaceWeatherPanel::onResize(int x, int y, int w, int h) {
  Widget::onResize(x, y, w, h);
  auto *cat = fontMgr_.catalog();
  labelFontSize_ = cat->ptSize(FontStyle::Fast);
  valueFontSize_ = cat->ptSize(FontStyle::SmallBold);
  destroyCache();
}

void SpaceWeatherPanel::onMouseMove(int mx, int my) {
  bool isNarrow = (width_ < 110);
  int numRows = isNarrow ? 4 : 2;
  int titleH = 20;
  bool showSparkline =
      (!isNarrow && xrayStore_ && !sparklineHistory_.empty() && height_ >= 80);
  int sparklineH = showSparkline ? 22 : 0;
  int cellH = (height_ - titleH - sparklineH - 10) / numRows;

  if (!showSparkline || sparklineHistory_.empty()) {
    tooltip_.visible = false;
    return;
  }

  // Sparkline area (must match render() logic)
  const int pad = 4;
  const int slX = x_ + pad;
  const int slW = width_ - 2 * pad;
  const int slY = y_ + titleH + numRows * cellH + 2;
  const int slH = sparklineH - 4;

  if (mx < slX || mx > slX + slW || my < slY || my > slY + slH) {
    tooltip_.visible = false;
    return;
  }

  // Find nearest data point based on X coordinate
  auto now = std::chrono::system_clock::now();
  auto tMin = now - std::chrono::hours(6);
  double tSpan = std::chrono::duration<double>(now - tMin).count();

  float bestDist = 99999.0f;
  const XRayDataPoint *bestPoint = nullptr;

  for (const auto &pt : sparklineHistory_) {
    if (pt.timestamp < tMin)
      continue;

    double age = std::chrono::duration<double>(pt.timestamp - tMin).count();
    int px = slX + static_cast<int>(age / tSpan * slW);
    float dist = std::abs(static_cast<float>(mx - px));
    if (dist < bestDist) {
      bestDist = dist;
      bestPoint = &pt;
    }
  }

  if (bestPoint && bestDist < 10.0f) {
    auto ageMins = std::chrono::duration_cast<std::chrono::minutes>(
                       now - bestPoint->timestamp)
                       .count();
    char buf[64];
    // Convert flux to Scientific notation
    if (ageMins < 5) {
      std::snprintf(buf, sizeof(buf), "%.1e W/m2 (Now)", bestPoint->flux);
    } else {
      std::snprintf(buf, sizeof(buf), "%.1e W/m2 (-%lldh %lldm)",
                    bestPoint->flux, static_cast<long long>(ageMins / 60),
                    static_cast<long long>(ageMins % 60));
    }
    tooltip_.text = buf;
    tooltip_.x = mx;
    tooltip_.y = my;
    tooltip_.visible = true;
    tooltip_.timestamp = SDL_GetTicks();
  } else {
    tooltip_.visible = false;
  }
}

bool SpaceWeatherPanel::onMouseUp(int mx, int my, Uint16 mod, int clicks) {
  (void)mx;
  (void)mod;
  // Don't consume clicks in the top 10% — let PaneContainer handle them
  // as widget-selection requests (same threshold used by PaneContainer).
  if (my - y_ < height_ / 10)
    return false;
  currentPage_ = (currentPage_ + 1) % 4;
  lastPageUpdate_ = SDL_GetTicks();
  destroyCache();
  return true;
}

std::vector<std::string> SpaceWeatherPanel::getActions() const {
  return {"cycle_page"};
}

SDL_Rect SpaceWeatherPanel::getActionRect(const std::string &action) const {
  if (action == "cycle_page") {
    // Return the whole widget to accept broad clicks
    return {x_, y_, width_, height_};
  }
  return {0, 0, 0, 0};
}

bool SpaceWeatherPanel::performAction(const std::string &action) {
  if (action == "cycle_page") {
    currentPage_ = (currentPage_ + 1) % 4;
    lastPageUpdate_ = SDL_GetTicks();
    destroyCache();
    return true;
  }
  return false;
}

nlohmann::json SpaceWeatherPanel::getDebugData() const {
  nlohmann::json j = nlohmann::json::object();
  for (const auto &item : items_) {
    if (item.label != "-" && !item.label.empty()) {
      j[item.label] = item.value;
    }
  }
  return j;
}
