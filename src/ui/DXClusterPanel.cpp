#include "DXClusterPanel.h"
#include "../core/ConfigManager.h"
#include "../core/LiveSpotData.h"
#include "../core/Logger.h"
#include "../services/RigService.h"
#include <SDL.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

DXClusterPanel::DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<DXClusterDataStore> store,
                               RigService *rigService, const AppConfig *config)
    : ListPanel(x, y, w, h, fontMgr, "DX Cluster", {}), store_(store),
      rigService_(rigService), config_(config) {}

void DXClusterPanel::update() {
  auto data = store_->snapshot();
  bool dataChanged = (data->lastUpdate != lastUpdate_);

  if (dataChanged) {
    rebuildRows(*data);
    lastUpdate_ = data->lastUpdate;
  }

  // Sync scroll offset and visible rows
  if (dataChanged || true) { // Always update visible slice if needed
    if (allRows_.empty()) {
      scrollOffset_ = 0;
    } else {
      int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
      scrollOffset_ = std::min(scrollOffset_, maxScroll);
    }

    std::vector<std::string> visible;
    visibleFreqs_.clear();

    if (allRows_.empty()) {
      visible.push_back(
          data->connected
              ? "Waiting for spots..."
              : (data->statusMsg.empty() ? "Disconnected" : data->statusMsg));
    } else {
      for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        int idx = scrollOffset_ + i;
        if (idx < (int)allRows_.size()) {
          visible.push_back(allRows_[idx]);
          visibleFreqs_.push_back(allFreqs_[idx]);
        }
      }
    }
    setRows(visible);

    // Update Highlight
    int highlighted = -1;
    if (data->hasSelection) {
      // Find selected spot in current visible slice
      for (int i = 0; i < (int)visibleFreqs_.size(); ++i) {
        int idx = scrollOffset_ + i;
        // In rebuildRows we reverse the spots, so DXClusterData::spots is not
        // directly indexed. We compare values.
        auto spots = data->spots;
        std::reverse(spots.begin(), spots.end());
        if (idx < (int)spots.size()) {
          const auto &spot = spots[idx];
          if (spot.txCall == data->selectedSpot.txCall &&
              spot.freqKhz == data->selectedSpot.freqKhz &&
              spot.spottedAt == data->selectedSpot.spottedAt) {
            highlighted = i;
            break;
          }
        }
      }
    }
    setHighlightedIndex(highlighted);
  }
}

void DXClusterPanel::rebuildRows(const DXClusterData &data) {
  allRows_.clear();
  allFreqs_.clear();
  auto spots = data.spots;
  // Most recent first
  std::reverse(spots.begin(), spots.end());

  for (const auto &spot : spots) {
    std::stringstream ss;
    // Format: "14025.0 K1ABC      5m"
    ss << std::fixed << std::setprecision(1) << std::setw(8) << spot.freqKhz
       << " " << std::left << std::setw(11) << spot.txCall << std::right
       << std::setw(4) << formatAge(spot.spottedAt);
    allRows_.push_back(ss.str());
    allFreqs_.push_back(spot.freqKhz);
  }
}

SDL_Color DXClusterPanel::getRowColor(int index,
                                      const SDL_Color &defaultColor) const {
  if (index >= 0 && index < (int)visibleFreqs_.size()) {
    int bandIdx = freqToBandIndex(visibleFreqs_[index]);
    if (bandIdx >= 0) {
      return kBands[bandIdx].color;
    }
  }
  return defaultColor;
}

std::string DXClusterPanel::formatAge(
    const std::chrono::system_clock::time_point &spottedAt) const {
  auto now = std::chrono::system_clock::now();
  auto age =
      std::chrono::duration_cast<std::chrono::minutes>(now - spottedAt).count();

  if (age < 0)
    return "0m";
  if (age < 60)
    return std::to_string(age) + "m";
  return std::to_string(age / 60) + "h";
}

bool DXClusterPanel::onMouseWheel(int scrollY) {
  if (allRows_.empty())
    return false;

  int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
  int newOffset = scrollOffset_ - scrollY;

  if (newOffset < 0)
    newOffset = 0;
  if (newOffset > maxScroll)
    newOffset = maxScroll;

  if (newOffset != scrollOffset_) {
    scrollOffset_ = newOffset;
    return true;
  }
  return false;
}

bool DXClusterPanel::onMouseUp(int mx, int my, Uint16 /*mod*/) {
  // Check if we clicked on a row
  int rowH = 14;
  auto font = fontMgr_.getFont(rowFontSize_);
  if (font)
    rowH = TTF_FontLineSkip(font);

  // ListPanel starts rendering after title. Adjust my.
  // We can just hit test visible rows from y_ + something.
  // For simplicity, let's use the same logic as ListPanel::render.
  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int curY = y_ + pad;
  // If we had a titleTex_, we add pad.
  // Let's assume title height is ~titleFontSize_ + pad.
  curY +=
      rowFontSize_ + pad; // Title is usually same font size as row or larger

  if (my < curY)
    return false;

  int clickedRow = (my - curY) / rowH;

  auto data = store_->snapshot();
  auto spots = data->spots;
  std::reverse(spots.begin(), spots.end());

  if (clickedRow >= 0 && clickedRow < (int)visibleFreqs_.size()) {
    int idx = scrollOffset_ + clickedRow;
    if (idx >= 0 && idx < (int)spots.size()) {
      const auto &spot = spots[idx];
      bool isSame = data->hasSelection &&
                    data->selectedSpot.txCall == spot.txCall &&
                    data->selectedSpot.freqKhz == spot.freqKhz &&
                    data->selectedSpot.spottedAt == spot.spottedAt;

      if (isSame) {
        store_->clearSelection();
      } else {
        store_->selectSpot(spot);

        // Auto-tune rig to spot frequency if enabled
        if (rigService_ && config_ && config_->rigAutoTune) {
          long long freqHz = static_cast<long long>(spot.freqKhz * 1000.0);
          rigService_->setFrequency(freqHz);
        }
      }
      return true;
    }
  }

  return false;
}

std::vector<std::string> DXClusterPanel::getActions() const {
  return {"open_setup", "scroll_up", "scroll_down"};
}

SDL_Rect DXClusterPanel::getActionRect(const std::string &action) const {
  if (action == "open_setup") {
    // Title area triggers setup?
    return {x_, y_, width_, 20};
  }
  return {0, 0, 0, 0};
}

bool DXClusterPanel::performAction(const std::string &action) {
  if (action == "scroll_up") {
    if (scrollOffset_ > 0) {
      scrollOffset_--;
      return true;
    }
  } else if (action == "scroll_down") {
    int maxScroll = std::max(0, (int)allRows_.size() - MAX_VISIBLE_ROWS);
    if (scrollOffset_ < maxScroll) {
      scrollOffset_++;
      return true;
    }
  } else if (action == "open_setup") {
    setupRequested_ = true;
    return true;
  }
  return false;
}

nlohmann::json DXClusterPanel::getDebugData() const {
  nlohmann::json j;
  auto data = store_->snapshot();
  j["connected"] = data->connected;
  j["spotCount"] = data->spots.size();
  j["scrollOffset"] = scrollOffset_;
  j["highlightedIndex"] = getHighlightedIndex();
  if (data->hasSelection) {
    j["selectedSpot"] = data->selectedSpot.txCall;
  }
  return j;
}
