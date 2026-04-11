#pragma once

#include "../core/WatchlistHitStore.h"
#include "../core/WatchlistStore.h"
#include "ListPanel.h"
#include <chrono>
#include <memory>

class WatchlistPanel : public ListPanel {
public:
  WatchlistPanel(int x, int y, int w, int h, FontManager &fontMgr,
                 std::shared_ptr<WatchlistStore> watchlist,
                 std::shared_ptr<WatchlistHitStore> hits);

  std::string getName() const override { return "Watchlist"; }
  const char *typeId() const override { return "watchlist"; }
  std::string getDisplayName() const override { return "Watchlist"; }
  bool isScrollable() const override { return true; }
  bool requiresConfigKey() const override { return false; }

  void update() override;
  bool onMouseWheel(int scrollY) override;

private:
  std::shared_ptr<WatchlistStore> watchlist_;
  std::shared_ptr<WatchlistHitStore> hits_;
  std::chrono::system_clock::time_point lastUpdate_{};

  std::vector<std::string> allRows_;
  int scrollOffset_ = 0;
  static constexpr int MAX_VISIBLE_ROWS = 12;
};
