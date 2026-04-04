#include "WatchlistPanel.h"
#include "WidgetRegistry.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

WatchlistPanel::WatchlistPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<WatchlistStore> watchlist,
                               std::shared_ptr<WatchlistHitStore> hits)
    : ListPanel(x, y, w, h, fontMgr, "Watchlist Hits", {}),
      watchlist_(watchlist), hits_(hits) {}

void WatchlistPanel::update() {
  auto last = hits_->lastUpdate();
  if (last != lastUpdate_) {
    allRows_.clear();
    auto hits = hits_->getHits();

    for (const auto &hit : hits) {
      std::stringstream ss;
      ss << std::left << std::setw(10) << hit.call << std::setw(8) << std::fixed
         << std::setprecision(1) << hit.freqKhz << " [" << hit.source << "]";
      allRows_.push_back(ss.str());
    }

    if (allRows_.empty()) {
      allRows_.push_back("Listening for...");
      auto calls = watchlist_->getAll();
      for (const auto &c : calls)
        allRows_.push_back("  " + c);
    }

    int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
    scrollOffset_ = std::min(scrollOffset_, maxScroll);
    int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allRows_.size());
    setRows(std::vector<std::string>(allRows_.begin() + scrollOffset_,
                                     allRows_.begin() + end));
    lastUpdate_ = last;
  }
}

bool WatchlistPanel::onMouseWheel(int scrollY) {
  if (allRows_.empty())
    return false;
  int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
  int newOffset = std::clamp(scrollOffset_ - scrollY, 0, maxScroll);
  if (newOffset == scrollOffset_)
    return false;
  scrollOffset_ = newOffset;
  int end = std::min(scrollOffset_ + MAX_VISIBLE_ROWS, (int)allRows_.size());
  setRows(std::vector<std::string>(allRows_.begin() + scrollOffset_,
                                   allRows_.begin() + end));
  return true;
}

REGISTER_WIDGET("watchlist", "Watchlist", false, false, {
  return std::make_unique<WatchlistPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.watchlistStore, deps.watchlistHitStore);
})
