#pragma once

#include <string>
#include <vector>

// One-click Contest Mode profile.
// Saves the current 6-pane layout and applies a DX-focused contest preset.
// Restores the previous layout on deactivation.
namespace ContestModeManager {

// Contest preset: one widget per pane, no rotation.
// Pane order matches AppConfig pane1..pane6.
static const std::vector<std::string> kContestLayout[6] = {
    {"dx_cluster"},
    {"live_spots"},
    {"band_conditions"},
    {"solar"},
    {"de_info"},
    {"dx_info"},
};

// Save current rotations to saved[], then apply contest preset.
inline void activate(std::vector<std::string> rotations[6],
                     std::vector<std::string> saved[6]) {
  for (int i = 0; i < 6; ++i) {
    saved[i]     = rotations[i];
    rotations[i] = kContestLayout[i];
  }
}

// Restore rotations[] from saved[].
inline void deactivate(std::vector<std::string> rotations[6],
                       const std::vector<std::string> saved[6]) {
  for (int i = 0; i < 6; ++i)
    rotations[i] = saved[i];
}

} // namespace ContestModeManager
