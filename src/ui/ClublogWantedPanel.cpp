#include "ClublogWantedPanel.h"
#include "WidgetRegistry.h"
#include "../core/MemoryMonitor.h"
#include <algorithm>

ClublogWantedPanel::ClublogWantedPanel(int x, int y, int w, int h,
                                       FontManager &fontMgr,
                                       std::shared_ptr<ClublogStore> clublogStore,
                                       std::shared_ptr<ADIFStore> adifStore)
    : ListPanel(x, y, w, h, fontMgr, "Most Wanted", {}),
      clublogStore_(std::move(clublogStore)),
      adifStore_(std::move(adifStore)) {}

ClublogWantedPanel::~ClublogWantedPanel() { clearRowCache(); }

void ClublogWantedPanel::clearRowCache() {
  for (auto &r : rowCache_) {
    if (r.rankTex)
      MemoryMonitor::getInstance().destroyTexture(r.rankTex);
    if (r.nameTex)
      MemoryMonitor::getInstance().destroyTexture(r.nameTex);
  }
  rowCache_.clear();
}

void ClublogWantedPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  clearRowCache();
}

void ClublogWantedPanel::update() {
  displayRows_.clear();
  rowH_ = std::max(12, static_cast<int>(height_ * 0.08f));

  if (!clublogStore_)
    return;

  auto entries = clublogStore_->get();
  ADIFStats stats;
  if (adifStore_)
    stats = adifStore_->get();

  for (const auto &entry : entries) {
    DisplayRow row;
    row.rank = entry.rank;
    row.name = entry.name;
    row.needed = (stats.valid && stats.workedEntitiesPerBand.find(entry.dxcc) ==
                                     stats.workedEntitiesPerBand.end());
    displayRows_.push_back(row);
  }

  rowCache_.resize(displayRows_.size());
}

void ClublogWantedPanel::render(SDL_Renderer *renderer) {
  if (!fontMgr_.ready() || !clublogStore_)
    return;

  auto entries = clublogStore_->get();
  if (entries.empty()) {
    ListPanel::render(renderer);
    if (fontMgr_.ready()) {
      int msgX = x_ + 10;
      int msgY = contentY_ + 10;
      int fontSize = std::max(10, height_ / 15);
      auto *tex = fontMgr_.renderText(renderer,
          "Configure Clublog API key\nin Setup > Services",
          getThemeColors(theme_).textDim, fontSize);
      if (tex) {
        int w, h;
        SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
        int maxW = width_ - 20;
        SDL_Rect dst = {msgX, msgY, w, h};
        if (w > maxW) {
          dst.w = maxW;
          dst.h = static_cast<int>(h * (maxW / static_cast<float>(w)));
        }
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        MemoryMonitor::getInstance().destroyTexture(tex);
      }
    }
    return;
  }

  ListPanel::render(renderer);

  ThemeColors themes = getThemeColors(theme_);
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

    SDL_Color color = row.needed ? themes.danger : themes.textDim;

    // Rank column
    if (!cache.rankTex || cache.lastRank != row.rank) {
      if (cache.rankTex)
        MemoryMonitor::getInstance().destroyTexture(cache.rankTex);
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%d", row.rank);
      cache.rankTex = fontMgr_.renderText(renderer, buf, color, rowH_ - 2,
                                          &cache.rankW, &cache.rankH);
      cache.lastRank = row.rank;
    }

    if (cache.rankTex) {
      int ty = y + (rowH_ - cache.rankH) / 2;
      SDL_Rect dst = {x_ + padX, ty, cache.rankW, cache.rankH};
      SDL_RenderCopy(renderer, cache.rankTex, nullptr, &dst);
    }

    // Name column
    if (!cache.nameTex || cache.lastName != row.name) {
      if (cache.nameTex)
        MemoryMonitor::getInstance().destroyTexture(cache.nameTex);
      cache.nameTex = fontMgr_.renderText(renderer, row.name, color, rowH_ - 2,
                                          &cache.nameW, &cache.nameH);
      cache.lastName = row.name;
    }

    if (cache.nameTex) {
      int ty = y + (rowH_ - cache.nameH) / 2;
      int nameX = x_ + padX + cache.rankW + 12;
      SDL_Rect dst = {nameX, ty, cache.nameW, cache.nameH};
      SDL_RenderCopy(renderer, cache.nameTex, nullptr, &dst);
    }
  }
}

REGISTER_WIDGET("clublog_wanted", "Most Wanted", true, false, {
  if (!deps.clublogStore)
    return nullptr;
  return std::make_unique<ClublogWantedPanel>(0, 0, 0, 0, deps.fontMgr,
                                              deps.clublogStore,
                                              deps.adifStore);
})
