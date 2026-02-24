#include "AsteroidPanel.h"
#include "../core/MemoryMonitor.h"
#include "../core/Theme.h"
#include <algorithm>
#include <cstdio>

AsteroidPanel::AsteroidPanel(int x, int y, int w, int h, FontManager &fontMgr,
                             AsteroidProvider &provider,
                             std::shared_ptr<HamClockState> state,
                             AppConfig *config,
                             std::function<void()> onSave)
    : ListPanel(x, y, w, h, fontMgr,
                (config && !config->asteroidIcon.empty()
                     ? config->asteroidIcon + " Asteroids"
                     : "☄ Asteroids"),
                {}),
      provider_(provider), state_(std::move(state)),
      config_(config), onSave_(std::move(onSave)) {
  onResize(x, y, w, h);
}

void AsteroidPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
}

void AsteroidPanel::update() {
  AsteroidData data = provider_.getLatest();
  // Trigger rebuild if a new fetch occurred OR if asteroids were filtered out
  // (size mismatch)
  if (data.valid &&
      (data.lastFetchTime != lastData_.lastFetchTime ||
       data.asteroids.size() != lastData_.asteroids.size() || rows_.empty())) {
    // If selected asteroid no longer in new data, clear selection
    if (selectedIndex_ >= 0 &&
        selectedIndex_ >= (int)data.asteroids.size()) {
      selectedIndex_ = -1;
      if (state_)
        state_->selectedAsteroidName.clear();
    }
    lastData_ = data;
    rebuildRows();
  }
  provider_.update(); // Background refresh logic (includes filtering)
}

void AsteroidPanel::rebuildRows() {
  std::vector<std::string> newRows;

  if (lastData_.asteroids.empty()) {
    newRows.push_back("No data available");
    setRows(newRows);
    return;
  }

  // Display top 4 asteroids (each takes 2 rows, total 8 rows)
  size_t count = std::min(lastData_.asteroids.size(), size_t(4));
  for (size_t i = 0; i < count; ++i) {
    const auto &ast = lastData_.asteroids[i];

    // Row 1: Name and Distance
    std::string name = ast.name;
    if (name.size() > 2 && name.front() == '(' && name.back() == ')') {
      name = name.substr(1, name.size() - 2);
    }
    // Truncate name if too long for a ~200px pane
    if (name.size() > 10)
      name = name.substr(0, 8) + "..";

    char row1[64];
    std::snprintf(row1, sizeof(row1), "%-10s %5.1f LD", name.c_str(),
                  ast.missDistanceLD);
    newRows.push_back(row1);

    // Row 2: Date, Time and Velocity
    char row2[64];
    std::string shortDate = ast.approachDate;
    if (shortDate.size() >= 10) {
      shortDate = shortDate.substr(5); // MM-DD
    }

    // Format: "MM-DD HH:MM  12km/s"
    std::snprintf(row2, sizeof(row2), "  %s %s  %2.0fkm/s", shortDate.c_str(),
                  ast.closeApproachTime.c_str(), ast.velocityKmS);
    newRows.push_back(row2);
  }

  setRows(newRows);
}

bool AsteroidPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (!state_ || lastData_.asteroids.empty())
    return false;

  // Replicate ListPanel layout math to determine which asteroid was tapped
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int titleBottom = y_ + pad + titleH_ + pad;
  int numRows = static_cast<int>(rows_.size());
  if (numRows == 0)
    return false;

  int remaining = (y_ + height_) - titleBottom;
  int rowH = std::max(rowFontSize_ + 4, remaining / numRows);

  if (my < titleBottom) {
    static const std::vector<std::string> kIcons = {"☄", "⊕", "◈", "⬡", "✦"};
    std::string cur = config_ ? config_->asteroidIcon : "☄";
    auto it = std::find(kIcons.begin(), kIcons.end(), cur);
    int idx = (it == kIcons.end()) ? 0 : (int)(it - kIcons.begin());
    idx = (idx + 1) % (int)kIcons.size();
    if (config_)
      config_->asteroidIcon = kIcons[idx];
    title_ = kIcons[idx] + " Asteroids";
    if (titleTex_) {
      MemoryMonitor::getInstance().destroyTexture(titleTex_);
      titleTex_ = nullptr;
    }
    if (onSave_)
      onSave_();
    return true;
  }

  int rowIdx = (my - titleBottom) / rowH;
  int astIdx = rowIdx / 2; // each asteroid occupies 2 rows

  if (astIdx < 0 || astIdx >= (int)lastData_.asteroids.size())
    return false;

  if (astIdx == selectedIndex_) {
    // Deselect
    selectedIndex_ = -1;
    state_->selectedAsteroidName.clear();
  } else {
    // Select
    selectedIndex_ = astIdx;
    state_->selectedAsteroidName = lastData_.asteroids[astIdx].name;
    provider_.fetchOrbitalElements(lastData_.asteroids[astIdx].name);
  }

  setHighlightedIndex(selectedIndex_ >= 0 ? selectedIndex_ * 2 : -1);
  return true;
}

SDL_Color AsteroidPanel::getRowColor(int index,
                                     const SDL_Color &defaultColor) const {
  if (index < 0 || index >= (int)rows_.size())
    return defaultColor;

  ThemeColors themes = getThemeColors(theme_);

  // Every even row is a header, every odd row is details
  bool isDetail = (index % 2 != 0);

  if (isDetail) {
    return themes.textDim;
  }

  // For the name row, check if hazardous
  int astIdx = index / 2;
  if (astIdx < (int)lastData_.asteroids.size()) {
    if (lastData_.asteroids[astIdx].isHazardous) {
      return themes.danger; // Official theme danger color (red)
    }
  }

  return themes.accent; // Cyan for names to match DX Cluster title style
}
