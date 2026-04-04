#include "AsteroidPanel.h"
#include "WidgetRegistry.h"
#include "TextureManager.h"
#include "../core/AsteroidPropagator.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include "FontCatalog.h"
#include "RenderUtils.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>

static const std::vector<std::string> kIcons = {"☄", "⊕", "◈", "⬡", "✦", "★",
                                                "✸", "⬢", "◉", "⊛", "✪", "⚡"};

static const SDL_Color kPalette[] = {
    {255, 140, 0, 255},   // orange
    {0, 255, 255, 255},   // cyan
    {0, 255, 128, 255},   // green
    {255, 60, 60, 255},   // red
    {255, 255, 255, 255}, // white
    {255, 230, 0, 255},   // yellow
};
static constexpr int kPaletteSize = 6;

AsteroidPanel::AsteroidPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             TextureManager &texMgr, AsteroidProvider &provider,
                             std::shared_ptr<HamClockState> state,
                             AppConfig *config, std::function<void()> onSave)
    : ListPanel(
          x, y, w, h, fontMgr,
          (config && !config->asteroidIcon.empty() ? "Asteroids" : "Asteroids"),
          {}),
      texMgr_(texMgr), provider_(provider), state_(std::move(state)),
      config_(config), onSave_(std::move(onSave)) {
  onResize(x, y, w, h);
}

void AsteroidPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  // Use Tiny font for more room in columns
  rowFontSize_ = fontMgr_.catalog()->ptSize(FontStyle::Tiny);
}

void AsteroidPanel::update() {
  AsteroidData data = provider_.getLatest();
  if (data.valid &&
      (data.lastFetchTime != lastData_.lastFetchTime ||
       data.asteroids.size() != lastData_.asteroids.size() || rows_.empty())) {
    if (selectedIndex_ >= 0 && selectedIndex_ >= (int)data.asteroids.size()) {
      selectedIndex_ = -1;
      if (state_)
        state_->selectedAsteroidName.clear();
    }
    lastData_ = data;
    rebuildRows();
  }
  provider_.update();

  // Pre-compute polar plot track when an asteroid is selected
  asteroidTrack_.clear();
  asteroidAboveHorizon_ = false;
  if (selectedIndex_ >= 0 && selectedIndex_ < (int)lastData_.asteroids.size() &&
      state_) {
    const auto &ast = lastData_.asteroids[selectedIndex_];
    OrbitalElements elem = provider_.getOrbitalElements(ast.name);
    if (elem.valid) {
      double obsLat = state_->deLocation.lat;
      double obsLon = state_->deLocation.lon;
      // Current JD
      double nowJd =
          static_cast<double>(std::time(nullptr)) / 86400.0 + 2440587.5;
      // 24 points over ±12h centred on closest approach
      double jd0 = (ast.julianDate > 0) ? ast.julianDate : nowJd;
      constexpr int kSteps = 24;
      constexpr double kSpanDays = 1.0; // ±12h = 1 day total
      asteroidTrack_.reserve(kSteps);
      for (int s = 0; s < kSteps; ++s) {
        double jd = jd0 - kSpanDays / 2.0 +
                    kSpanDays * s / static_cast<double>(kSteps - 1);
        double subLat, subLon;
        if (AsteroidPropagator::subAsteroidPoint(elem, jd, subLat, subLon)) {
          double az, el;
          AsteroidPropagator::computeAzEl(obsLat, obsLon, subLat, subLon, az,
                                          el);
          asteroidTrack_.push_back({az, el});
        }
      }
      // Current position
      double subLat, subLon;
      if (AsteroidPropagator::subAsteroidPoint(elem, nowJd, subLat, subLon)) {
        AsteroidPropagator::computeAzEl(obsLat, obsLon, subLat, subLon,
                                        asteroidCurrentAzEl_.az,
                                        asteroidCurrentAzEl_.el);
        asteroidAboveHorizon_ = (asteroidCurrentAzEl_.el > 0.0);
      }
    }
  }
}

void AsteroidPanel::rebuildRows() {
  std::vector<std::string> newRows;
  allRowToAstIndex_.clear();
  rowToAstIndex_.clear();

  if (lastData_.asteroids.empty()) {
    newRows.push_back("No data available");
    setRows(newRows);
    return;
  }

  size_t count = lastData_.asteroids.size();

  // Build display order: selected asteroid first (if any), then the rest.
  if (selectedIndex_ >= 0 && selectedIndex_ < (int)count) {
    allRowToAstIndex_.push_back(selectedIndex_);
  }
  for (size_t i = 0; i < count; ++i) {
    if ((int)i != selectedIndex_)
      allRowToAstIndex_.push_back(static_cast<int>(i));
  }

  // Clamp scroll and slice visible window
  int maxScroll = std::max(0, (int)allRowToAstIndex_.size() - MAX_VISIBLE_ROWS);
  scrollOffset_ = std::min(scrollOffset_, maxScroll);
  int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allRowToAstIndex_.size());
  rowToAstIndex_ = std::vector<int>(allRowToAstIndex_.begin() + scrollOffset_,
                                     allRowToAstIndex_.begin() + end);

  auto formatRow = [&](int astIdx) {
    const auto &ast = lastData_.asteroids[astIdx];
    std::string name = ast.name;
    if (name.size() > 2 && name.front() == '(' && name.back() == ')')
      name = name.substr(1, name.size() - 2);
    if (name.size() > 14)
      name = name.substr(0, 14);
    // approachDate format: "YYYY-Mon-DD" (e.g. "2026-Feb-19")
    std::string monStr =
        ast.approachDate.size() >= 8 ? ast.approachDate.substr(5, 3) : "???";
    std::string day =
        ast.approachDate.size() >= 11 ? ast.approachDate.substr(9, 2) : "??";
    char row[128];
    std::snprintf(row, sizeof(row), "%s|%s|%s|%s", name.c_str(), monStr.c_str(),
                  day.c_str(), ast.closeApproachTime.c_str());
    return std::string(row);
  };

  for (int idx : rowToAstIndex_)
    newRows.push_back(formatRow(idx));

  setRows(newRows);
}

bool AsteroidPanel::onMouseWheel(int scrollY) {
  if (allRowToAstIndex_.empty())
    return false;
  int maxScroll = std::max(0, (int)allRowToAstIndex_.size() - MAX_VISIBLE_ROWS);
  int newOffset = std::clamp(scrollOffset_ - scrollY, 0, maxScroll);
  if (newOffset == scrollOffset_)
    return false;
  scrollOffset_ = newOffset;
  rebuildRows();
  return true;
}

void AsteroidPanel::render(SDL_Renderer *renderer) {
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int settingsH = rowFontSize_ * 2 + pad * 5;
  int origH = height_;

  // Allocate polar plot area when track data is available
  bool hasPolar = !asteroidTrack_.empty();
  int nonSettingsH = origH - settingsH;
  int polarH = 0;
  if (hasPolar) {
    polarH = std::min(width_, nonSettingsH * 2 / 5);
    polarH = std::max(polarH, 60); // minimum useful size
    if (polarH > nonSettingsH - 20)
      polarH = 0; // not enough room, skip
  }

  // Let ListPanel render with reduced height (list + polar)
  height_ -= settingsH + polarH;
  ListPanel::render(renderer);
  height_ = origH;

  ThemeColors themes = getThemeColors(theme_);

  // Repaint full-widget border (covers ListPanel's reduced-height border)
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_Rect fullRect = {x_, y_, width_, height_};
  SDL_RenderDrawRect(renderer, &fullRect);

  int settingsY = y_ + height_ - settingsH;

  // Polar plot (between list and settings footer)
  if (hasPolar && polarH > 0) {
    int plotTop = settingsY - polarH;
    // Draw divider above polar area
    SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                           themes.border.b, themes.border.a);
    SDL_RenderDrawLine(renderer, x_ + 1, plotTop, x_ + width_ - 2, plotTop);
    // Fill polar background
    SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                           themes.bg.a);
    SDL_Rect polarBg = {x_ + 1, plotTop + 1, width_ - 2, polarH - 1};
    SDL_RenderFillRect(renderer, &polarBg);

    int radius = std::min(width_ / 2, polarH / 2) - pad - 4;
    if (radius > 8) {
      float cx = x_ + width_ * 0.5f;
      float cy = static_cast<float>(plotTop) + polarH * 0.5f;
      renderPolarPlot(renderer, cx, cy, radius);
    }
  }

  // Fill settings background
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b,
                         themes.bg.a);
  SDL_Rect settingsBg = {x_ + 1, settingsY, width_ - 2, settingsH - 1};
  SDL_RenderFillRect(renderer, &settingsBg);

  // Divider line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, themes.border.a);
  SDL_RenderDrawLine(renderer, x_ + 1, settingsY, x_ + width_ - 2, settingsY);

  // --- Icon row ---
  int iconRowY = settingsY + pad;
  int iconSlotW = (width_ - 2 * pad) / (int)kIcons.size();
  std::string curIcon = config_ ? config_->asteroidIcon : "☄";

  for (int k = 0; k < (int)kIcons.size(); ++k) {
    int slotX = x_ + pad + k * iconSlotW;
    bool isCurrent = (kIcons[k] == curIcon);

    if (isCurrent) {
      // Highlight rect
      SDL_Color hi = themes.accent;
      hi.a = 120;
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(renderer, hi.r, hi.g, hi.b, hi.a);
      SDL_Rect hiRect = {slotX, iconRowY, iconSlotW, rowFontSize_ + pad};
      SDL_RenderFillRect(renderer, &hiRect);
      SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    int iw = 0, ih = 0;
    SDL_Color iconColor = isCurrent ? themes.accent : themes.textDim;
    SDL_Texture *t = fontMgr_.renderText(renderer, kIcons[k], iconColor,
                                         rowFontSize_, &iw, &ih);
    if (t) {
      SDL_Rect dst = {slotX + (iconSlotW - iw) / 2,
                      iconRowY + (rowFontSize_ + pad - ih) / 2, iw, ih};
      SDL_RenderCopy(renderer, t, nullptr, &dst);
      SDL_DestroyTexture(t);
    }
  }

  // --- Color swatch row ---
  int swatchRowY = iconRowY + rowFontSize_ + pad * 2;
  int swatchSize = rowFontSize_;
  int totalSwatchW = kPaletteSize * (swatchSize + pad) - pad;
  int swatchStartX = x_ + (width_ - totalSwatchW) / 2;

  SDL_Color curColor =
      config_ ? config_->asteroidColor : themes.warning;

  for (int k = 0; k < kPaletteSize; ++k) {
    int sx = swatchStartX + k * (swatchSize + pad);
    SDL_Color sc = kPalette[k];
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, sc.r, sc.g, sc.b, sc.a);
    SDL_Rect sr = {sx, swatchRowY, swatchSize, swatchSize};
    SDL_RenderFillRect(renderer, &sr);

    bool selected =
        (sc.r == curColor.r && sc.g == curColor.g && sc.b == curColor.b);
    if (selected) {
      SDL_SetRenderDrawColor(renderer, themes.text.r, themes.text.g,
                             themes.text.b, 255);
      SDL_RenderDrawRect(renderer, &sr);
    }
  }
}

bool AsteroidPanel::onMouseUp(int mx, int my, Uint16 /*mod*/, int clicks) {
  if (!state_ || lastData_.asteroids.empty())
    return false;

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int settingsH = rowFontSize_ * 2 + pad * 5;
  int settingsY = y_ + height_ - settingsH;
  int titleBottom = y_ + pad + titleH_ + pad;
  int numRows = static_cast<int>(rows_.size());

  // Top 10% → widget selection (let PaneContainer handle it)
  if (my < y_ + height_ / 10)
    return false;

  // Title area → pass through (no icon cycling here)
  if (my < titleBottom)
    return false;

  // Settings footer
  if (my >= settingsY) {
    int iconRowY = settingsY + pad;
    int iconRowBottom = iconRowY + rowFontSize_ + pad;

    // Icon row
    if (my < iconRowBottom) {
      int iconSlotW = (width_ - 2 * pad) / (int)kIcons.size();
      int relX = mx - (x_ + pad);
      int iconIdx = relX / iconSlotW;
      if (iconIdx >= 0 && iconIdx < (int)kIcons.size()) {
        if (config_)
          config_->asteroidIcon = kIcons[iconIdx];
        title_ = "Asteroids";
        if (titleTex_) {
          MemoryMonitor::getInstance().destroyTexture(titleTex_);
          titleTex_ = nullptr;
        }
        if (onSave_)
          onSave_();
      }
      return true;
    }

    // Color swatch row
    int swatchRowY = iconRowBottom + pad;
    int swatchSize = rowFontSize_;
    int totalSwatchW = kPaletteSize * (swatchSize + pad) - pad;
    int swatchStartX = x_ + (width_ - totalSwatchW) / 2;

    if (my >= swatchRowY && my < swatchRowY + swatchSize) {
      for (int k = 0; k < kPaletteSize; ++k) {
        int sx = swatchStartX + k * (swatchSize + pad);
        if (mx >= sx && mx < sx + swatchSize) {
          if (config_)
            config_->asteroidColor = kPalette[k];
          if (onSave_)
            onSave_();
          break;
        }
      }
    }
    return true;
  }

  // List rows (1 row per asteroid)
  if (numRows == 0 || rowToAstIndex_.empty())
    return false;

  int listH = settingsY - titleBottom;
  int rowH = std::max(rowFontSize_ + 4, listH / numRows);
  int rowIdx = (my - titleBottom) / rowH;

  if (rowIdx < 0 || rowIdx >= (int)rowToAstIndex_.size())
    return false;

  int astIdx = rowToAstIndex_[rowIdx];

  if (astIdx == selectedIndex_) {
    // Deselect: restore original order
    selectedIndex_ = -1;
    state_->selectedAsteroidName.clear();
  } else {
    selectedIndex_ = astIdx;
    state_->selectedAsteroidName = lastData_.asteroids[astIdx].name;
    provider_.fetchOrbitalElements(lastData_.asteroids[astIdx].name);
  }

  // Rebuild rows so selection moves to top (or is removed)
  rebuildRows();
  // After rebuild, selected row is always 0 (or none)
  setHighlightedIndex(selectedIndex_ >= 0 ? 0 : -1);
  return true;
}

void AsteroidPanel::renderPolarPlot(SDL_Renderer *renderer, float cx, float cy,
                                    int radius) {
  ThemeColors themes = getThemeColors(theme_);
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kDeg2Rad = kPi / 180.0;
  static const char *kCompassLabels[4] = {"N", "E", "S", "W"};

  // --- Concentric elevation rings (90°/60°/30°/0° = horizon) ---
  SDL_Color ringColor = themes.border;
  ringColor.a = 80;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  for (int elev = 0; elev <= 60; elev += 30) {
    float r = static_cast<float>(radius) * (90.0f - elev) / 90.0f;
    constexpr int kSegs = 64;
    std::vector<SDL_FPoint> pts;
    pts.reserve(kSegs + 1);
    for (int s = 0; s <= kSegs; ++s) {
      double theta = 2.0 * kPi * s / kSegs;
      pts.push_back({cx + r * static_cast<float>(std::cos(theta)),
                     cy + r * static_cast<float>(std::sin(theta))});
    }
    RenderUtils::drawPolyline(renderer, pts.data(),
                              static_cast<int>(pts.size()), 1.0f, ringColor,
                              false);
  }

  // --- Radial lines (N/E/S/W) ---
  for (int i = 0; i < 4; ++i) {
    double angle = i * 90.0 * kDeg2Rad;
    float ex = cx + static_cast<float>(radius * std::sin(angle));
    float ey = cy - static_cast<float>(radius * std::cos(angle));
    RenderUtils::drawThickLine(renderer, cx, cy, ex, ey, 1.0f, ringColor);
  }

  // --- Compass labels ---
  if (fontMgr_.catalog()) {
    SDL_Color dimGray = themes.textDim;
    int labelDist = radius + 4;
    for (int i = 0; i < 4; ++i) {
      double angle = i * 90.0 * kDeg2Rad;
      int lx = static_cast<int>(cx + labelDist * std::sin(angle)) - 3;
      int ly = static_cast<int>(cy - labelDist * std::cos(angle)) - 5;
      fontMgr_.catalog()->drawText(renderer, kCompassLabels[i], lx, ly, dimGray,
                                   FontStyle::Fast);
    }
  }

  // --- Asteroid track arc (above-horizon segments only, orange) ---
  SDL_Color trackColor = {255, 140, 0, 255};
  SDL_Texture *lineAA = texMgr_.get("line_aa");
  std::vector<SDL_FPoint> seg;
  for (const auto &p : asteroidTrack_) {
    if (p.el > 0.0) {
      double r = static_cast<double>(radius) * (90.0 - p.el) / 90.0;
      seg.push_back({cx + static_cast<float>(r * std::sin(p.az * kDeg2Rad)),
                     cy - static_cast<float>(r * std::cos(p.az * kDeg2Rad))});
    } else {
      if (seg.size() >= 2) {
        if (lineAA)
          RenderUtils::drawPolylineTextured(renderer, lineAA, seg.data(),
                                            (int)seg.size(), 2.0f, trackColor);
        else
          RenderUtils::drawPolyline(renderer, seg.data(),
                                    static_cast<int>(seg.size()), 2.0f,
                                    trackColor, false);
      }
      seg.clear();
    }
  }
  if (seg.size() >= 2) {
    if (lineAA)
      RenderUtils::drawPolylineTextured(renderer, lineAA, seg.data(),
                                        (int)seg.size(), 2.0f, trackColor);
    else
      RenderUtils::drawPolyline(renderer, seg.data(),
                                static_cast<int>(seg.size()), 2.0f, trackColor,
                                false);
  }

  // --- Current position marker (if above horizon) ---
  if (asteroidAboveHorizon_) {
    double r =
        static_cast<double>(radius) * (90.0 - asteroidCurrentAzEl_.el) / 90.0;
    float sx = cx + static_cast<float>(
                        r * std::sin(asteroidCurrentAzEl_.az * kDeg2Rad));
    float sy = cy - static_cast<float>(
                        r * std::cos(asteroidCurrentAzEl_.az * kDeg2Rad));
    float markerR = std::max(3.0f, radius / 15.0f);
    RenderUtils::drawCircle(renderer, sx, sy, markerR, trackColor);
  }
}

SDL_Color AsteroidPanel::getRowColor(int index,
                                     const SDL_Color &defaultColor) const {
  ThemeColors themes = getThemeColors(theme_);

  int astIdx = (index >= 0 && index < (int)rowToAstIndex_.size())
                   ? rowToAstIndex_[index]
                   : index;
  if (astIdx >= 0 && astIdx < (int)lastData_.asteroids.size() &&
      lastData_.asteroids[astIdx].isHazardous)
    return themes.danger;

  // Use the default (white) color for normal rows.
  // Accent/highlight color is applied by ListPanel for the selected row.
  return defaultColor;
}

void AsteroidPanel::renderRowText(SDL_Renderer *renderer, int index, int rx,
                                  int ry, int rw, int rh, SDL_Color color) {
  if (index < 0 || index >= (int)rows_.size())
    return;
  std::string text = rows_[index];

  // Split by |
  std::vector<std::string> cols;
  size_t start = 0, end = 0;
  while ((end = text.find('|', start)) != std::string::npos) {
    cols.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  cols.push_back(text.substr(start));

  if (cols.size() < 4) {
    // Fallback if not a piped row (e.g. "No data available")
    ListPanel::renderRowText(renderer, index, rx, ry, rw, rh, color);
    return;
  }

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));

  // Column start X positions (percentages of rw)
  // Name (48%), Month (20%), Day (14%), Time (18%)
  float colX[] = {0.0f, 0.48f, 0.68f, 0.82f};

  for (size_t i = 0; i < cols.size() && i < 4; ++i) {
    int tw = 0, th = 0;
    SDL_Texture *tex =
        fontMgr_.renderText(renderer, cols[i], color, rowFontSize_, &tw, &th);
    if (tex) {
      int tx = rx + pad + static_cast<int>((rw - 2 * pad) * colX[i]);
      int ty = ry + (rh - th) / 2;
      SDL_Rect dst = {tx, ty, tw, th};
      SDL_RenderCopy(renderer, tex, nullptr, &dst);
      SDL_DestroyTexture(tex);
    }
  }
}

REGISTER_WIDGET("asteroid", "Asteroids", false, false, {
  return std::make_unique<AsteroidPanel>(0, 0, 0, 0, deps.fontMgr, deps.texMgr,
      *deps.asteroidProvider, deps.state, &deps.appCfg,
      [&appCfg = deps.appCfg, &cfgMgr = deps.cfgMgr]() { cfgMgr.save(appCfg); });
})
