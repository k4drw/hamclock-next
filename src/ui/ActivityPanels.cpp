#include "ActivityPanels.h"
#include <iomanip>
#include <sstream>

// --- DXPedPanel ---

DXPedPanel::DXPedPanel(int x, int y, int w, int h, FontManager &fontMgr,
                       std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Peditions", {}), store_(store) {}

void DXPedPanel::update() {
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
                     std::shared_ptr<ActivityDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "On The Air", {}), store_(store) {}

void ONTAPanel::update() {
  auto data = store_->get();
  if (data.lastUpdated != lastUpdate_) {
    std::vector<std::string> rows;
    // Format: "MODE  CALL     REF (PROG)"
    for (const auto &os : data.ontaSpots) {
      std::stringstream ss;
      ss << std::left << std::setw(6) << os.mode << std::setw(10) << os.call
         << os.ref << " (" << os.program << ")";
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
