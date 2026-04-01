#include "DXClusterPanel.h"
#include "WidgetRegistry.h"
#include "../core/ADIFData.h"
#include "../core/ConfigManager.h"
#include "../core/LiveSpotData.h"
#include "../core/MemoryMonitor.h"
#include "../services/RigService.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

// Returns IARU Region 2 sub-band mode label for a given frequency in kHz.
static const char *modeFromFreq(double khz) {
  // FT8 dial frequencies (±0.6 kHz)
  static const double ft8f[] = {1840, 3573, 7074, 10136, 14074,
                                18100, 21074, 24915, 28074, 50313};
  for (double f : ft8f)
    if (std::abs(khz - f) < 0.6) return "FT8";
  // FT4 dial frequencies (±0.6 kHz)
  static const double ft4f[] = {3575, 7047, 14080, 18104, 21140, 24919, 28180};
  for (double f : ft4f)
    if (std::abs(khz - f) < 0.6) return "FT4";
  // WSPR dial frequencies (±0.4 kHz)
  static const double wsprf[] = {1836.6, 3592.6, 7038.6, 10138.7,
                                  14095.6, 18104.6, 21094.6, 24924.6, 28124.6};
  for (double f : wsprf)
    if (std::abs(khz - f) < 0.4) return "WSPR";
  // Band plan sub-bands
  if (khz >= 1800  && khz < 1843)  return "CW";
  if (khz >= 1843  && khz < 2000)  return "SSB";
  if (khz >= 3500  && khz < 3570)  return "CW";
  if (khz >= 3570  && khz < 3600)  return "RTTY";
  if (khz >= 3600  && khz < 4000)  return "SSB";
  if (khz >= 7000  && khz < 7040)  return "CW";
  if (khz >= 7040  && khz < 7125)  return "RTTY";
  if (khz >= 7125  && khz < 7300)  return "SSB";
  if (khz >= 10100 && khz < 10130) return "CW";
  if (khz >= 10130 && khz < 10150) return "RTTY";
  if (khz >= 14000 && khz < 14070) return "CW";
  if (khz >= 14070 && khz < 14100) return "RTTY";
  if (khz >= 14100 && khz < 14350) return "SSB";
  if (khz >= 18068 && khz < 18095) return "CW";
  if (khz >= 18095 && khz < 18110) return "RTTY";
  if (khz >= 18110 && khz < 18168) return "SSB";
  if (khz >= 21000 && khz < 21070) return "CW";
  if (khz >= 21070 && khz < 21150) return "RTTY";
  if (khz >= 21150 && khz < 21450) return "SSB";
  if (khz >= 24890 && khz < 24915) return "CW";
  if (khz >= 24915 && khz < 24990) return "SSB";
  if (khz >= 28000 && khz < 28070) return "CW";
  if (khz >= 28070 && khz < 28300) return "RTTY";
  if (khz >= 28300 && khz < 29700) return "SSB";
  if (khz >= 50000 && khz < 50100) return "CW";
  if (khz >= 50100 && khz < 50300) return "SSB";
  return "";
}

static SDL_Color modeColor(const char *mode) {
  if (!mode || mode[0] == '\0') return {120, 120, 120, 255};
  std::string m(mode);
  if (m == "CW")   return {220, 200,  60, 255}; // yellow
  if (m == "SSB")  return { 80, 160, 255, 255}; // blue
  if (m == "FT8")  return { 60, 210,  80, 255}; // green
  if (m == "FT4")  return {255, 140,  40, 255}; // orange
  if (m == "RTTY") return {230,  90,  50, 255}; // red-orange
  if (m == "WSPR") return {180,  80, 255, 255}; // purple
  return {160, 160, 160, 255};
}

DXClusterPanel::DXClusterPanel(int x, int y, int w, int h, FontManager &fontMgr,
                               std::shared_ptr<DXClusterDataStore> store,
                               RigService *rigService, const AppConfig *config,
                               std::shared_ptr<ADIFStore> adifStore)
    : ListPanel(x, y, w, h, fontMgr, "DX Cluster", {}), store_(store),
      adifStore_(std::move(adifStore)), rigService_(rigService), config_(config) {}

DXClusterPanel::~DXClusterPanel() { clearSpotCache(); }

void DXClusterPanel::clearSpotCache() {
  for (auto &cs : spotCache_) {
    if (cs.freqTex)
      MemoryMonitor::getInstance().destroyTexture(cs.freqTex);
    if (cs.modeTex)
      MemoryMonitor::getInstance().destroyTexture(cs.modeTex);
    if (cs.badgeTex)
      MemoryMonitor::getInstance().destroyTexture(cs.badgeTex);
    if (cs.callTex)
      MemoryMonitor::getInstance().destroyTexture(cs.callTex);
    if (cs.ageTex)
      MemoryMonitor::getInstance().destroyTexture(cs.ageTex);
  }
  spotCache_.clear();
}

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
    visibleSpots_.clear();

    if (allRows_.empty()) {
      visible.push_back(
          data->connected
              ? "Waiting for spots..."
              : (data->statusMsg.empty() ? "Disconnected" : data->statusMsg));
    } else {
      for (int i = 0; i < MAX_VISIBLE_ROWS; ++i) {
        int idx = scrollOffset_ + i;
        if (idx < (int)allRows_.size()) {
          // Push empty string so ListPanel draws stripes but no text
          visible.push_back("");
          visibleFreqs_.push_back(allFreqs_[idx]);
          visibleSpots_.push_back(allSpots_[idx]);
        }
      }
    }
    setRows(visible);

    // Resize spot cache
    if (spotCache_.size() != visibleSpots_.size()) {
      clearSpotCache();
      spotCache_.resize(visibleSpots_.size());
    }

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

void DXClusterPanel::onResize(int x, int y, int w, int h) {
  ListPanel::onResize(x, y, w, h);
  clearSpotCache();
}

void DXClusterPanel::render(SDL_Renderer *renderer) {
  // Base render for BG, Title, Border
  ListPanel::render(renderer);

  if (visibleSpots_.empty())
    return;

  if (!fontMgr_.ready())
    return;

  int pad = std::max(2, static_cast<int>(width_ * 0.03f));
  int curY = y_ + pad;
  if (titleTex_) {
    curY += titleH_ + pad;
  }

  // Column layout: Freq | [Mode] | [Badge] | Call | Age
  // Mode badge (CW/FT8/SSB…) shown at ≥140px; DXCC badge (N/B) shown at ≥120px.
  int freqColW = fontMgr_.getLogicalWidth("88888.8", rowFontSize_);
  int ageColW = fontMgr_.getLogicalWidth("999m", rowFontSize_);
  bool showMode = (width_ >= 140);
  bool showBadge = (width_ >= 120) && adifStore_ && adifStore_->get().valid;
  int modeColW =
      showMode ? (fontMgr_.getLogicalWidth("RTTY", rowFontSize_) + 2) : 0;
  int badgeColW =
      showBadge ? (fontMgr_.getLogicalWidth("B", rowFontSize_) + 2) : 0;
  int freqXEnd = x_ + pad + freqColW;
  int modeX = freqXEnd + 4;
  int badgeX = modeX + modeColW + (showMode ? 4 : 0);
  int callX = badgeX + badgeColW + (showBadge ? 3 : (showMode ? 0 : 2));
  int ageX = x_ + width_ - pad - ageColW;

  // Band Legend at bottom
  int legendH = (height_ >= 120) ? 28 : 0;
  legendH_ = legendH;
  // spdlog::info("DXCluster: height_={}, legendH_={}", height_, legendH_);

  // Calculate row height (compact remaining space)
  int remaining = (y_ + height_ - legendH) - curY;
  int rowH = std::max(rowFontSize_ + 4,
                      remaining / static_cast<int>(visibleSpots_.size()));
  contentY_ = curY;
  rowH_ = rowH;

  for (size_t i = 0; i < visibleSpots_.size(); ++i) {
    if (i >= spotCache_.size())
      break;

    int rowY = curY + static_cast<int>(i) * rowH;
    if (rowY + rowH > y_ + height_ - legendH)
      break;
    const auto &spot = visibleSpots_[i];
    auto &cache = spotCache_[i];
    SDL_Color color = getRowColor(i, getThemeColors(theme_).text);

    bool freqChanged = std::abs(cache.lastFreq - spot.freq) > 0.001;

    // 1. Freq (right-aligned within freq column)
    if (!cache.freqTex || freqChanged) {
      if (cache.freqTex)
        MemoryMonitor::getInstance().destroyTexture(cache.freqTex);
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.1f", spot.freq);
      cache.freqTex = fontMgr_.renderText(renderer, buf, color, rowFontSize_,
                                          &cache.freqW, &cache.freqH);
      cache.lastFreq = spot.freq;
    }
    if (cache.freqTex) {
      int ty = rowY + (rowH - cache.freqH) / 2;
      SDL_Rect dst = {freqXEnd - cache.freqW, ty, cache.freqW, cache.freqH};
      SDL_Rect clip = {x_ + pad, rowY, freqColW, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.freqTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
    }

    // 2. Mode badge (left-aligned in mode column, only when wide enough)
    if (showMode) {
      std::string modeStr(modeFromFreq(spot.freq));
      if (!cache.modeTex || freqChanged || cache.lastMode != modeStr) {
        if (cache.modeTex)
          MemoryMonitor::getInstance().destroyTexture(cache.modeTex);
        cache.modeTex = nullptr;
        cache.modeW = cache.modeH = 0;
        if (!modeStr.empty()) {
          SDL_Color mc = modeColor(modeStr.c_str());
          cache.modeTex = fontMgr_.renderText(renderer, modeStr, mc,
                                              rowFontSize_, &cache.modeW, &cache.modeH);
        }
        cache.lastMode = modeStr;
      }
      if (cache.modeTex) {
        int ty = rowY + (rowH - cache.modeH) / 2;
        SDL_Rect dst = {modeX, ty, cache.modeW, cache.modeH};
        SDL_Rect clip = {modeX, rowY, modeColW, rowH};
        SDL_RenderSetClipRect(renderer, &clip);
        SDL_RenderCopy(renderer, cache.modeTex, nullptr, &dst);
        SDL_RenderSetClipRect(renderer, nullptr);
      }
    }

    // 3. DXCC needed badge (N=new entity, B=new band)
    if (showBadge && spot.txDxcc > 0) {
      ADIFStats adif = adifStore_->get();
      auto it = adif.workedEntitiesPerBand.find(spot.txDxcc);
      std::string badgeStr;
      SDL_Color badgeCol = {120, 120, 120, 255};
      if (it == adif.workedEntitiesPerBand.end()) {
        badgeStr = "N"; // new entity — not in log at all
        badgeCol = {0, 240, 220, 255}; // cyan
      } else {
        int bandIdx = freqToBandIndex(spot.freq);
        if (bandIdx >= 0) {
          std::string band(kBands[bandIdx].name);
          if (it->second.find(band) == it->second.end()) {
            badgeStr = "B"; // new band
            badgeCol = {255, 220, 60, 255}; // yellow
          }
        }
      }
      if (!cache.badgeTex || cache.lastBadge != badgeStr) {
        if (cache.badgeTex)
          MemoryMonitor::getInstance().destroyTexture(cache.badgeTex);
        cache.badgeTex = nullptr;
        cache.badgeW = cache.badgeH = 0;
        if (!badgeStr.empty()) {
          cache.badgeTex = fontMgr_.renderText(renderer, badgeStr, badgeCol,
                                               rowFontSize_, &cache.badgeW, &cache.badgeH);
        }
        cache.lastBadge = badgeStr;
      }
      if (cache.badgeTex) {
        int ty = rowY + (rowH - cache.badgeH) / 2;
        SDL_Rect dst = {badgeX, ty, cache.badgeW, cache.badgeH};
        SDL_RenderCopy(renderer, cache.badgeTex, nullptr, &dst);
      }
    }

    // 4. Call (left-aligned, clipped before age column)
    if (!cache.callTex || cache.lastCall != spot.call) {
      if (cache.callTex)
        MemoryMonitor::getInstance().destroyTexture(cache.callTex);
      cache.callTex = fontMgr_.renderText(renderer, spot.call, color,
                                          rowFontSize_, &cache.callW, &cache.callH);
      cache.lastCall = spot.call;
    }
    if (cache.callTex) {
      int ty = rowY + (rowH - cache.callH) / 2;
      SDL_Rect dst = {callX, ty, cache.callW, cache.callH};
      SDL_Rect clip = {callX, rowY, ageX - callX - 2, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.callTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
    }

    // 5. Age (right-anchored, always visible)
    std::string age = formatAge(spot.time);
    if (!cache.ageTex || cache.lastAge != age) {
      if (cache.ageTex)
        MemoryMonitor::getInstance().destroyTexture(cache.ageTex);
      cache.ageTex = fontMgr_.renderText(renderer, age, color, rowFontSize_,
                                         &cache.ageW, &cache.ageH);
      cache.lastAge = age;
    }
    if (cache.ageTex) {
      int ty = rowY + (rowH - cache.ageH) / 2;
      SDL_Rect dst = {x_ + width_ - pad - cache.ageW, ty, cache.ageW, cache.ageH};
      SDL_Rect clip = {ageX, rowY, ageColW, rowH};
      SDL_RenderSetClipRect(renderer, &clip);
      SDL_RenderCopy(renderer, cache.ageTex, nullptr, &dst);
      SDL_RenderSetClipRect(renderer, nullptr);
    }
  }

  // Band Legend at bottom (drawn last to ensure it borders the scrollable
  // content)
  if (legendH_ > 0) {
    renderBandLegend(renderer, curY, y_ + height_ - 2);
  }
}

void DXClusterPanel::rebuildRows(const DXClusterData &data) {
  allRows_.clear();
  allFreqs_.clear();
  allSpots_.clear();
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

    allSpots_.push_back({spot.txCall, spot.freqKhz, spot.spottedAt, spot.txDxcc});
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

bool DXClusterPanel::onMouseUp(int mx, int my, Uint16 /*mod*/, int clicks) {
  // Use cached geometry from render() so click rows match visual rows exactly.
  // Return false for clicks above content area — PaneContainer handles those
  // (widget selector fires if click is in top 10% of the pane).
  if (my < contentY_)
    return false;

  int clickedRow = (my - contentY_) / std::max(1, rowH_);

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
        if (onSpotDeactivated_)
          onSpotDeactivated_();
      } else {
        store_->selectSpot(spot);

        // Auto-tune rig to spot frequency if enabled
        if (rigService_ && config_ && config_->rigAutoTune) {
          long long freqHz = static_cast<long long>(spot.freqKhz * 1000.0);
          rigService_->setFrequency(freqHz);
        }
        if (onSpotActivated_)
          onSpotActivated_(spot);
      }
      return true;
    }
  }

  // Absorb all clicks in the content area so they don't fall through to
  // PaneContainer's onSelectionRequested_ and open the adjacent pane's editor.
  return true;
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

void DXClusterPanel::renderBandLegend(SDL_Renderer *renderer, int & /*curY*/,
                                      int maxY) {
  int cellH = 14;  // Tiny font target 12px + 2px padding
  int legendH = cellH * 2;  // 28px for 2 rows
  int legendY = maxY - legendH;

  ThemeColors themes = getThemeColors(theme_);

  // Background for the legend area to prevent spot overlap (fully opaque)
  SDL_Rect legendRect = {x_ + 1, legendY, width_ - 2, legendH};
  SDL_SetRenderDrawColor(renderer, themes.bg.r, themes.bg.g, themes.bg.b, 255);
  SDL_RenderFillRect(renderer, &legendRect);

  // Top border/separator line
  SDL_SetRenderDrawColor(renderer, themes.border.r, themes.border.g,
                         themes.border.b, 200);
  SDL_RenderDrawLine(renderer, x_ + 1, legendY, x_ + width_ - 1, legendY);

  int cols = 6;
  int cellW = (width_ - 4) / cols;
  int boxSize = 7;

  for (int i = 0; i < kNumBands; ++i) {
    int row = i / cols;
    int col = i % cols;
    int lx = x_ + 4 + col * cellW;
    int midY = legendY + row * cellH + cellH / 2;

    // Colored square, vertically centered in the row
    SDL_Rect box = {lx + 1, midY - boxSize / 2, boxSize, boxSize};
    SDL_SetRenderDrawColor(renderer, kBands[i].color.r, kBands[i].color.g,
                           kBands[i].color.b, 255);
    SDL_RenderFillRect(renderer, &box);

    // Label right of box, vertically centered on the same midline (strip
    // trailing 'm')
    std::string label(kBands[i].name);
    if (!label.empty() && label.back() == 'm')
      label.pop_back();
    fontMgr_.catalog()->drawText(
        renderer, label, lx + boxSize + 1, midY, themes.text, FontStyle::Tiny,
        /*centered=*/false, /*rightAlign=*/false, /*vertCentered=*/true);
  }
}

#ifndef __EMSCRIPTEN__
REGISTER_WIDGET("dx_cluster", "DX Cluster", true, false, {
  return std::make_unique<DXClusterPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.dxcStore, deps.rigService, &deps.appCfg, deps.adifStore);
})
#else
REGISTER_WIDGET("dx_cluster", "DX Cluster", true, false, {
  return std::make_unique<DXClusterPanel>(
      0, 0, 0, 0, deps.fontMgr, deps.dxcStore, nullptr, &deps.appCfg, deps.adifStore);
})
#endif
