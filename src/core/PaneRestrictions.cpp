#include "PaneRestrictions.h"
#include <algorithm>
#include <vector>

namespace HamClock {

bool PaneRestrictions::isWidgetAllowed(int paneIdx, const std::string &typeId) {
  // Pane 3 corresponds to Display Pane 4 (small, top-right).
  if (paneIdx == 3) {
    const std::vector<std::string> &allowed = getAllowedWidgets(3);
    return std::find(allowed.begin(), allowed.end(), typeId) != allowed.end();
  }
  return true; // All other panes allow all widgets (currently).
}

std::vector<std::string> PaneRestrictions::getAllowedWidgets(int paneIdx) {
  if (paneIdx == 3) {
    return {"ncdxf", "solar", "dx_weather", "de_weather", "band_conditions"};
  }
  return {}; // No restriction (all allowed).
}

} // namespace HamClock
