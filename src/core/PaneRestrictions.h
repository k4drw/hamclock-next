#pragma once
#include <string>
#include <vector>

namespace HamClock {

class PaneRestrictions {
public:
  /**
   * Returns true if the widget typeId is allowed in the given pane index (0-5).
   * Pane 3 (Display Pane 4) has a strict subset of allowed widgets due to its size.
   */
  static bool isWidgetAllowed(int paneIdx, const std::string &typeId);

  /**
   * Returns the list of allowed widgets for a given pane.
   * If empty, all registered widgets are considered allowed.
   */
  static std::vector<std::string> getAllowedWidgets(int paneIdx);
};

} // namespace HamClock
