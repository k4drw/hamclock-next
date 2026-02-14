#include "DXClusterPanel.h"
#include <iomanip>
#include <sstream>

DXClusterPanel::DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<DXClusterDataStore> store)
    : ListPanel(x, y, w, h, fontMgr, "DX Cluster", {}), store_(store) {}

void DXClusterPanel::update() {
  auto data = store_->get();
  bool dataChanged = (data.lastUpdate != lastUpdate_);

  if (dataChanged) {
    rebuildRows(data);
    lastUpdate_ = data.lastUpdate;
  }

  // Re-render visible rows if data changed or scrolled (though ListPanel
  // handles its own render tick, we update contents here)
  if (dataChanged) {
    // Ensure scroll offset is valid
    if (allRows_.empty()) {
      scrollOffset_ = 0;
    } else {
      int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
      scrollOffset_ = std::min(scrollOffset_, maxScroll);
    }

    // Update ListPanel rows
    std::vector<std::string> visible;
    if (allRows_.empty()) {
      visible.push_back(
          data.connected
              ? "Waiting for spots..."
              : (data.statusMsg.empty() ? "Disconnected" : data.statusMsg));
    } else {
      for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        if (scrollOffset_ + i < (int)allRows_.size()) {
          visible.push_back(allRows_[scrollOffset_ + i]);
        }
      }
    }
    setRows(visible);
  }
}

void DXClusterPanel::rebuildRows(const DXClusterData &data) {
  allRows_.clear();
  auto spots = data.spots;
  // Most recent first
  std::reverse(spots.begin(), spots.end());

  for (const auto &spot : spots) {
    std::stringstream ss;
    // Format: "14025.0 K1ABC      5m"
    // Freq: 8 chars
    // Call: 11 chars
    // Age:  4 chars (right aligned)
    ss << std::fixed << std::setprecision(1) << std::setw(8) << spot.freqKhz
       << " " << std::left << std::setw(11) << spot.txCall << std::right
       << std::setw(4) << formatAge(spot.spottedAt);
    allRows_.push_back(ss.str());
  }
}

std::string DXClusterPanel::formatAge(
    const std::chrono::system_clock::time_point &spottedAt) const {
  auto now = std::chrono::system_clock::now();
  auto age =
      std::chrono::duration_cast<std::chrono::minutes>(now - spottedAt).count();

  if (age < 0)
    return "0m"; // Future spot?
  if (age < 60)
    return std::to_string(age) + "m";
  return std::to_string(age / 60) + "h";
}

bool DXClusterPanel::onMouseWheel(int scrollY) {
  if (allRows_.empty())
    return false;

  int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
  int newOffset = scrollOffset_ - scrollY; // Scroll up decreases offset

  if (newOffset < 0)
    newOffset = 0;
  if (newOffset > maxScroll)
    newOffset = maxScroll;

  if (newOffset != scrollOffset_) {
    scrollOffset_ = newOffset;

    // Immediate update of visible rows
    std::vector<std::string> visible;
    for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
      if (scrollOffset_ + i < (int)allRows_.size()) {
        visible.push_back(allRows_[scrollOffset_ + i]);
      }
    }
    setRows(visible);
    return true;
  }
  return false;
}

bool DXClusterPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  if (my > y_ + height_ / 2) {
    // Check if we clicked on a row
    int headerH = 20; // Approx header height if we had one, but we don't.
                      // Adjust based on where list starts.
    // ListPanel renders from y_ + something.
    // Let's assume standard list rendering geometry.
    // If ListPanel used similar logic to what we see in other panels:
    // It renders visible rows starting at y_.

    int rowH = 14;
    auto font = fontMgr_.getFont(rowFontSize_);
    if (font)
      rowH = TTF_FontLineSkip(font);

    int relY = my - y_;
    int clickedRow = relY / rowH;

    auto data = store_->get();
    // Reverse logic matches rebuildRows
    // We need to access the sorted spots as displayed.
    // Rebuilding them here is inefficient but safe.
    auto spots = data.spots;
    std::reverse(spots.begin(), spots.end());

    if (clickedRow >= 0 && clickedRow < MAX_VISIBLE_ROWS) {
      int idx = scrollOffset_ + clickedRow;
      if (idx >= 0 && idx < (int)spots.size()) {
        const auto &spot = spots[idx];
        bool isSame = data.hasSelection &&
                      data.selectedSpot.txCall == spot.txCall &&
                      data.selectedSpot.freqKhz == spot.freqKhz &&
                      data.selectedSpot.spottedAt == spot.spottedAt;

        if (isSame) {
          store_->clearSelection();
        } else {
          store_->selectSpot(spot);
        }
        return true;
      }
    }
  }

  // Keep the setup request check for bottom half if valid,
  // but wait... the logic "my > y_ + height_ / 2" was for "setupRequested_".
  // The user wanted "click on Counts to bring up options" for LiveSpots.
  // For DXCluster, "setup" might be less critical or handled differently.
  // The original code used the bottom half click for setup.
  // We should preserve that OR maybe move it to a specific button?
  // User asked: "is it possible for the DX Cluster to NOT plot all spots on the
  // map and only plot those clicked on in the widget?" Accessing the bottom
  // half for setup makes selecting rows in the bottom half impossible. Let's
  // REMOVE the big "bottom half = setup" hit area and rely on Semantic API or
  // specific UI element if needed. Or, since we don't have a "Setup" button
  // visible, maybe we add one or rely on the "DX Cluster" title click? For now,
  // let's prioritize the Row Selection.

  return false;
}

std::vector<std::string> DXClusterPanel::getActions() const {
  return {"open_setup", "scroll_up", "scroll_down"};
}

SDL_Rect DXClusterPanel::getActionRect(const std::string &action) const {
  if (action == "open_setup") {
    // Bottom half triggers setup
    return {x_, y_ + height_ / 2, width_, height_ / 2};
  }
  return {0, 0, 0, 0};
}

nlohmann::json DXClusterPanel::getDebugData() const {
  nlohmann::json j;
  auto data = store_->get();
  j["connected"] = data.connected;
  j["spotCount"] = data.spots.size();
  j["scrollOffset"] = scrollOffset_;
  if (!data.spots.empty()) {
    j["lastSpotFreq"] = data.spots.back().freqKhz;
    j["lastSpotCall"] = data.spots.back().txCall;
  }
  return j;
}
