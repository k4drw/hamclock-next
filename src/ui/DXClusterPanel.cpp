#include "DXClusterPanel.h"
#include <iomanip>
#include <sstream>

DXClusterPanel::DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<DXClusterDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Cluster", {}), store_(store) {}

void DXClusterPanel::update() {
  auto data = store_->get();
  if (data.lastUpdate != lastUpdate_) {
    std::vector<std::string> rows;

    // Most recent first
    auto spots = data.spots;
    std::reverse(spots.begin(), spots.end());

    for (const auto &spot : spots) {
      std::stringstream ss;
      // Format: "14025.0 K1ABC"
      ss << std::fixed << std::setprecision(1) << std::setw(8) << spot.freqKhz
         << " " << std::left << std::setw(12) << spot.txCall;

      // Add time if we have room, or rx call
      // For now just freq and call
      rows.push_back(ss.str());
      if (rows.size() >= 15)
        break;
    }

    if (rows.empty()) {
      if (data.connected) {
        rows.push_back("Waiting for spots...");
      } else {
        rows.push_back(data.statusMsg.empty() ? "Disconnected"
                                              : data.statusMsg);
      }
    }

    setRows(rows);
    lastUpdate_ = data.lastUpdate;
  }
}

bool DXClusterPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (my > y_ + height_ / 2) {
    setupRequested_ = true;
    return true;
  }
  return false;
}
