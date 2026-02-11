#pragma once

#include "Widget.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <vector>

enum class Zone {
  TopBar,
  SidePanel,
  MainStage,
};

class LayoutManager {
public:
  // weight: relative size within the zone (default 1.0).
  // TopBar: higher weight = wider slot.  SidePanel: higher weight = taller
  // slot.
  void addWidget(Zone zone, Widget *widget, float weight = 1.0f) {
    entries_.push_back({zone, widget, weight});
  }

  void setFidelityMode(bool on) { fidelityMode_ = on; }
  bool fidelityMode() const { return fidelityMode_; }

  void recalculate(int winW, int winH, int offX = 0, int offY = 0) {
    winW_ = winW;
    winH_ = winH;

    if (fidelityMode_) {
      recalculateFidelity(offX, offY);
      return;
    }

    // TopBar: 22% of height, min 110px (fits clock digits + data rows)
    int topH = std::max(110, static_cast<int>(winH * 0.22f));

    // SidePanel: 17% of width, min 160px (always visible)
    int sideW = std::max(160, static_cast<int>(winW * 0.17f));

    // MainStage fills the rest
    int mainX = sideW;
    int mainY = topH;
    int mainW = winW - sideW;
    int mainH = winH - topH;

    // Sum weights per zone
    float topWeight = 0, sideWeight = 0;
    for (auto &e : entries_) {
      if (e.zone == Zone::TopBar)
        topWeight += e.weight;
      else if (e.zone == Zone::SidePanel)
        sideWeight += e.weight;
    }

    // Distribute widgets within each zone using weights
    float topAccum = 0, sideAccum = 0;
    for (auto &entry : entries_) {
      switch (entry.zone) {
      case Zone::TopBar: {
        float frac = (topWeight > 0) ? entry.weight / topWeight : 0;
        int slotX = static_cast<int>(topAccum / topWeight * winW);
        int slotW = static_cast<int>(frac * winW);
        entry.widget->onResize(slotX, 0, slotW, topH);
        topAccum += entry.weight;
        break;
      }
      case Zone::SidePanel: {
        float frac = (sideWeight > 0) ? entry.weight / sideWeight : 0;
        int slotY = topH + static_cast<int>(sideAccum / sideWeight * mainH);
        int slotH = static_cast<int>(frac * mainH);
        entry.widget->onResize(0, slotY, sideW, slotH);
        sideAccum += entry.weight;
        break;
      }
      case Zone::MainStage: {
        entry.widget->onResize(mainX, mainY, mainW, mainH);
        break;
      }
      }
    }
  }

  int windowWidth() const { return winW_; }
  int windowHeight() const { return winH_; }

private:
  // Canonical rects from hamclock_layout.md (800x480 logical coords).
  // Assigns exact pixel positions to each widget by registration order within
  // each zone. Canonical rects from hamclock_layout.md (800x480 logical
  // coords). Assigns exact pixel positions to each widget by registration order
  // within each zone.
  void recalculateFidelity(int offX, int offY) {
    // Canonical rects: {x, y, w, h}
    // TopBar widgets (order: TimePanel, SpaceWx, DXCluster, LiveSpots,
    // BandCond)
    static constexpr SDL_Rect kTopBar[] = {
        {0, 0, 235, 148},   // TimePanel (Callsign+Clock+Aux)
        {235, 0, 160, 148}, // SpaceWx (Pane 1)
        {405, 0, 160, 148}, // DX Cluster (Pane 2)
        {575, 0, 160, 148}, // Live Spots (Pane 3)
        {738, 0, 62, 148},  // BandCond (NCDXF/Status)
    };
    // SidePanel widgets (order: LocalPanel, DXSatPane)
    static constexpr SDL_Rect kSide[] = {
        {0, 148, 139, 147}, // LocalPanel (top half)
        {0, 295, 139, 185}, // DXSatPane (bottom half)
    };
    // MainStage
    static constexpr SDL_Rect kMain = {139, 149, 660, 330};

    int topIdx = 0, sideIdx = 0;
    for (auto &entry : entries_) {
      switch (entry.zone) {
      case Zone::TopBar:
        if (topIdx < static_cast<int>(std::size(kTopBar))) {
          auto &r = kTopBar[topIdx++];
          entry.widget->onResize(r.x + offX, r.y + offY, r.w, r.h);
        }
        break;
      case Zone::SidePanel:
        if (sideIdx < static_cast<int>(std::size(kSide))) {
          auto &r = kSide[sideIdx++];
          entry.widget->onResize(r.x + offX, r.y + offY, r.w, r.h);
        }
        break;
      case Zone::MainStage:
        entry.widget->onResize(kMain.x + offX, kMain.y + offY, kMain.w,
                               kMain.h);
        break;
      }
    }
  }

  struct Entry {
    Zone zone;
    Widget *widget;
    float weight;
  };

  std::vector<Entry> entries_;
  int winW_ = 0;
  int winH_ = 0;
  bool fidelityMode_ = false;
};
