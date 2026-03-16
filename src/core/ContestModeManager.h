#pragma once

#include "WidgetType.h"
#include <vector>

// One-click Contest Mode profile.
// Saves the current 6-pane layout and applies a DX-focused contest preset.
// Restores the previous layout on deactivation.
namespace ContestModeManager {

// Contest preset: one widget per pane, no rotation.
// Pane order matches AppConfig pane1..pane6.
static const std::vector<WidgetType> kContestLayout[6] = {
    {WidgetType::DX_CLUSTER},
    {WidgetType::LIVE_SPOTS},
    {WidgetType::BAND_CONDITIONS},
    {WidgetType::SOLAR},
    {WidgetType::DE_INFO},
    {WidgetType::DX_INFO},
};

// Save current rotations to saved[], then apply contest preset.
inline void activate(std::vector<WidgetType> rotations[6],
                     std::vector<WidgetType> saved[6]) {
  for (int i = 0; i < 6; ++i) {
    saved[i]     = rotations[i];
    rotations[i] = kContestLayout[i];
  }
}

// Restore rotations[] from saved[].
inline void deactivate(std::vector<WidgetType> rotations[6],
                       const std::vector<WidgetType> saved[6]) {
  for (int i = 0; i < 6; ++i)
    rotations[i] = saved[i];
}

} // namespace ContestModeManager
