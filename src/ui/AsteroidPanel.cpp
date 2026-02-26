#include "AsteroidPanel.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include <algorithm>
#include <cstdio>

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
                             AsteroidProvider &provider,
                             std::shared_ptr<HamClockState> state,
                             AppConfig *config, std::function<void()> onSave)
    : ListPanel(
          x, y, w, h, fontMgr,
          (config && !config->asteroidIcon.empty() ? "Asteroids" : "Asteroids"),
          {}),
      provider_(provider), state_(std::move(state)), config_(config),
      onSave_(std::move(onSave)) {
  onResize(x, y, w, h);
}

void AsteroidPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
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
}

void AsteroidPanel::rebuildRows() {
  std::vector<std::string> newRows;

  if (lastData_.asteroids.empty()) {
    newRows.push_back("No data available");
    setRows(newRows);
    return;
  }

  size_t count = std::min(lastData_.asteroids.size(), size_t(7));
  for (size_t i = 0; i < count; ++i) {
    const auto &ast = lastData_.asteroids[i];
    std::string name = ast.name;
    if (name.size() > 2 && name.front() == '(' && name.back() == ')')
      name = name.substr(1, name.size() - 2);
    if (name.size() > 8)
      name = name.substr(0, 8);

    // approachDate format: "YYYY-Mon-DD" (e.g. "2026-Feb-19")
    std::string monStr =
        ast.approachDate.size() >= 8 ? ast.approachDate.substr(5, 3) : "???";
    std::string day =
        ast.approachDate.size() >= 11 ? ast.approachDate.substr(9, 2) : "??";

    char row[64];
    std::snprintf(row, sizeof(row), "%-8s %s %s %s", name.c_str(),
                  monStr.c_str(), day.c_str(), ast.closeApproachTime.c_str());
    newRows.push_back(row);
  }

  setRows(newRows);
}

void AsteroidPanel::render(SDL_Renderer *renderer) {
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int settingsH = rowFontSize_ * 2 + pad * 5;
  int origH = height_;

  // Let ListPanel render with reduced height (no settings area)
  height_ -= settingsH;
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
      config_ ? config_->asteroidColor : SDL_Color{255, 140, 0, 255};

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
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
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
  if (numRows == 0)
    return false;

  int listH = settingsY - titleBottom;
  int rowH = std::max(rowFontSize_ + 4, listH / numRows);
  int rowIdx = (my - titleBottom) / rowH;
  int astIdx = rowIdx; // 1 row per asteroid

  if (astIdx < 0 || astIdx >= (int)lastData_.asteroids.size())
    return false;

  if (astIdx == selectedIndex_) {
    selectedIndex_ = -1;
    state_->selectedAsteroidName.clear();
  } else {
    selectedIndex_ = astIdx;
    state_->selectedAsteroidName = lastData_.asteroids[astIdx].name;
    provider_.fetchOrbitalElements(lastData_.asteroids[astIdx].name);
  }

  setHighlightedIndex(selectedIndex_ >= 0 ? selectedIndex_ : -1);
  return true;
}

SDL_Color AsteroidPanel::getRowColor(int index,
                                     const SDL_Color &defaultColor) const {
  ThemeColors themes = getThemeColors(theme_);

  if (index >= 0 && index < (int)lastData_.asteroids.size() &&
      lastData_.asteroids[index].isHazardous)
    return themes.danger;

  return themes.accent;
}
