#include "WatchlistPanel.h"
#include "WidgetRegistry.h"
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
    std::vector<std::string> rows;
    auto hits = hits_->getHits();

    for (const auto &hit : hits) {
      std::stringstream ss;
      ss << std::left << std::setw(10) << hit.call << std::setw(8) << std::fixed
         << std::setprecision(1) << hit.freqKhz << " [" << hit.source << "]";
      rows.push_back(ss.str());
      if (rows.size() >= 12)
        break;
    }

    if (rows.empty()) {
      rows.push_back("Listening for...");
      auto calls = watchlist_->getAll();
      for (const auto &c : calls) {
        rows.push_back("  " + c);
        if (rows.size() >= 10)
          break;
      }
    }

    setRows(rows);
    lastUpdate_ = last;
  }
}

REGISTER_WIDGET("watchlist", "Watchlist", false, false, {
  return std::make_unique<WatchlistPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.watchlistStore, deps.watchlistHitStore);
})
