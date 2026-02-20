#pragma once

#include <mutex>
#include <string>
#include <vector>

class CitiesManager {
public:
  static CitiesManager &getInstance();

  void init();

  // Find name of nearest city within a reasonable distance (e.g. 50km)
  // Returns empty string if none found.
  std::string findNearest(float lat, float lon, float *dist_miles = nullptr);

private:
  CitiesManager() = default;
  std::mutex mutex_;
  bool initialized_ = false;

  // Spatial grid for fast lookups. Grid is 180x360 cells (1-degree resolution).
  // Each cell contains a vector of indices into the g_CityData array.
  std::vector<std::vector<std::vector<size_t>>> grid_;
};
