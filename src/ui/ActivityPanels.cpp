#include "ActivityPanels.h"
#include <iomanip>
#include <sstream>

// --- DXPedPanel ---

DXPedPanel::DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       ActivityProvider &provider,
                       std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Peditions", {}), provider_(provider),
      store_(store) {}

void DXPedPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 20 * 60 * 1000 || lastFetch_ == 0) { // 20 mins
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    std::vector<std::string> rows;
    // Format: "CALL        LOCATION"
    for (const auto &de : data.dxpeds) {
      std::stringstream ss;
      ss << std::left << std::setw(12) << de.call << de.location;
      rows.push_back(ss.str());
      if (rows.size() >= 10)
        break;
    }
    if (rows.empty() && data.valid) {
      rows.push_back("No upcoming expeditions");
    }
    setRows(rows);
    lastUpdate_ = data.lastUpdated;
  }
}

// --- ONTAPanel ---

ONTAPanel::ONTAPanel(int x, int y, int w, int h, FontManager &fontMgr,
                     ActivityProvider &provider,
                     std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "On The Air", {}), provider_(provider),
      store_(store) {}

void ONTAPanel::update() {
  uint32_t nowTicks = SDL_GetTicks();
  if (nowTicks - lastFetch_ > 5 * 60 * 1000 || lastFetch_ == 0) { // 5 mins
    lastFetch_ = nowTicks;
    provider_.fetch();
  }

  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    // Update title to reflect current filter
    static const char *kFilterNames[] = {"ALL", "POTA", "SOTA"};
    static const char *kFilterPrograms[] = {nullptr, "POTA", "SOTA"};
    std::string newTitle =
        std::string("On The Air [") + kFilterNames[filterMode_] + "]";
    if (newTitle != title_) {
      title_ = newTitle;
      destroyCache();
    }

    std::vector<std::string> rows;
    // Format: "MODE  CALL     REF (PROG)"
    const char *filterProg = kFilterPrograms[filterMode_];
    for (const auto &os : data.ontaSpots) {
      if (filterProg && os.program != filterProg)
        continue;
      std::stringstream ss;
      ss << std::left << std::setw(6) << os.mode << std::setw(10) << os.call
         << os.ref;
      if (filterMode_ == 0)
        ss << " (" << os.program << ")";
      rows.push_back(ss.str());
      if (rows.size() >= 12)
        break;
    }
    if (rows.empty() && data.valid) {
      rows.push_back("No active spots");
    }
    setRows(rows);
    lastUpdate_ = data.lastUpdated;
  }
}

bool ONTAPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  // Click anywhere in the title area (top ~20% of widget) cycles the filter
  if (my >= y_ && my < y_ + height_ / 5) {
    filterMode_ = (filterMode_ + 1) % 3;
    // Force a re-render of title and rows
    lastUpdate_ = {};
    return true;
  }
  return false;
}
