#include "GridProgressPanel.h"
#include "WidgetRegistry.h"
#include "../core/MemoryMonitor.h"
#include <algorithm>

GridProgressPanel::GridProgressPanel(int x, int y, int w, int h,
                                     FontManager &fontMgr,
                                     std::shared_ptr<ADIFStore> adifStore)
    : ListPanel(x, y, w, h, fontMgr, "Grid Progress", {}),
      adifStore_(std::move(adifStore)) {}

GridProgressPanel::~GridProgressPanel() { clearRowCache(); }

void GridProgressPanel::clearRowCache() {
  for (auto &r : rowCache_) {
    if (r.gridTex)
      MemoryMonitor::getInstance().destroyTexture(r.gridTex);
    if (r.statusTex)
      MemoryMonitor::getInstance().destroyTexture(r.statusTex);
  }
  rowCache_.clear();
}

void GridProgressPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  clearRowCache();
}

void GridProgressPanel::update() {
  displayRows_.clear();
  rowH_ = std::max(12, static_cast<int>(height_ * 0.08f));

  if (!adifStore_)
    return;

  auto stats = adifStore_->get();
  if (!stats.valid)
    return;

  // Build grid display rows: sort by grid, show worked grids
  for (const auto &[grid, isWorked] : stats.workedGrids4) {
    if (!isWorked)
      continue; // Only show worked grids
    GridDisplayRow row;
    row.grid4 = grid;
    row.confirmed = stats.confirmedGrids4.count(grid) > 0;
    displayRows_.push_back(row);
  }

  std::sort(displayRows_.begin(), displayRows_.end(),
            [](const GridDisplayRow &a, const GridDisplayRow &b) {
              return a.grid4 < b.grid4;
            });

  // Adjust cache size
  rowCache_.resize(displayRows_.size());
}

void GridProgressPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready() || !adifStore_)
    return;

  auto stats = adifStore_->get();
  if (!stats.valid)
    return;

  // Title bar
  SDL_Color titleColor = getThemeColors(theme_).text;
  int worked = stats.workedGrids4.size();
  int confirmed = stats.confirmedGrids4.size();
  char title[64];
  std::snprintf(title, sizeof(title), "Grid Progress: %d worked, %d confirmed",
                worked, confirmed);

  ListPanel::render(renderer);  // Base rendering (BG, border, etc)

  int padX = 4;
  int rowY = contentY_;

  for (size_t i = 0; i < displayRows_.size(); ++i) {
    if (i >= rowCache_.size())
      break;

    int y = rowY + static_cast<int>(i) * rowH_;
    if (y + rowH_ > y_ + height_)
      break;

    const auto &row = displayRows_[i];
    auto &cache = rowCache_[i];

    SDL_Color color = getRowColor(i, titleColor);

    // Grid square column
    if (!cache.gridTex || cache.lastGrid != row.grid4) {
      if (cache.gridTex)
        MemoryMonitor::getInstance().destroyTexture(cache.gridTex);
      cache.gridTex = fontMgr_.renderText(renderer, row.grid4, color, rowH_ - 2,
                                          &cache.gridW, &cache.gridH);
      cache.lastGrid = row.grid4;
    }

    if (cache.gridTex) {
      int ty = y + (rowH_ - cache.gridH) / 2;
      SDL_Rect dst = {x_ + padX, ty, cache.gridW, cache.gridH};
      SDL_RenderCopy(renderer, cache.gridTex, nullptr, &dst);
    }

    // Status column (Worked/Confirmed)
    std::string status = row.confirmed ? "✓" : "W";
    SDL_Color statusCol = row.confirmed ? (SDL_Color{0, 220, 100, 255})
                                        : (SDL_Color{200, 200, 200, 255});

    if (!cache.statusTex || cache.lastConfirmed != row.confirmed) {
      if (cache.statusTex)
        MemoryMonitor::getInstance().destroyTexture(cache.statusTex);
      cache.statusTex = fontMgr_.renderText(renderer, status, statusCol,
                                            rowH_ - 2, &cache.statusW, &cache.statusH);
      cache.lastConfirmed = row.confirmed;
    }

    if (cache.statusTex) {
      int ty = y + (rowH_ - cache.statusH) / 2;
      int sx = x_ + width_ - padX - cache.statusW;
      SDL_Rect dst = {sx, ty, cache.statusW, cache.statusH};
      SDL_RenderCopy(renderer, cache.statusTex, nullptr, &dst);
    }
  }
}

// Self-registration via REGISTER_WIDGET macro
REGISTER_WIDGET("grid_progress", "Grid Progress", true, false, {
  if (!deps.adifStore)
    return nullptr;
  return std::make_unique<GridProgressPanel>(0, 0, 0, 0, deps.fontMgr,
                                             deps.adifStore);
})
