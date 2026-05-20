#pragma once

#include <functional>

/**
 * @brief Context passed to widgets for rendering overlays on the map.
 * This decouples widgets from the MapWidget implementation while allowing them
 * to project coordinates.
 */
struct MapContext {
  // Functions to convert longitude/latitude to screen coordinates (within the map widget).
  std::function<int(double)> lonToScreenX;
  std::function<int(double)> latToScreenY;
};
