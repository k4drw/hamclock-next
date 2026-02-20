#include "CitiesManager.h"
#include "CitiesData.h"
#include "Logger.h"
#include <cmath>
#include <vector>

constexpr int GRID_LAT_SIZE = 180;
constexpr int GRID_LON_SIZE = 360;

CitiesManager &CitiesManager::getInstance() {
  static CitiesManager instance;
  return instance;
}

void CitiesManager::init() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (initialized_)
    return;

  // Resize the grid to its full dimensions
  grid_.resize(GRID_LAT_SIZE);
  for (int i = 0; i < GRID_LAT_SIZE; ++i) {
    grid_[i].resize(GRID_LON_SIZE);
  }

  // Populate the grid with indices from the static city data array
  for (size_t i = 0; i < g_CityDataSize; ++i) {
    const auto &city = g_CityData[i];
    // Normalize lat/lon to grid indices
    int lat_idx = static_cast<int>(city.lat + 90.0f) % GRID_LAT_SIZE;
    int lon_idx = static_cast<int>(city.lon + 180.0f) % GRID_LON_SIZE;
    
    if (lat_idx >= 0 && lat_idx < GRID_LAT_SIZE && lon_idx >= 0 && lon_idx < GRID_LON_SIZE) {
        grid_[lat_idx][lon_idx].push_back(i);
    }
  }
  
  initialized_ = true;
  LOG_I("CitiesManager", "Initialized with {} static cities into a {}x{} spatial grid.", g_CityDataSize, GRID_LAT_SIZE, GRID_LON_SIZE);
}

// Simple squared Euclidean distance in degrees (corrected for longitude)
// Sufficient for finding the nearest neighbor.
static float get_dist_sq(float lat1, float lon1, float lat2, float lon2) {
  float dy = lat1 - lat2;
  float dx = (lon1 - lon2) * std::cos(lat1 * 0.0174532925f);
  return dy * dy + dx * dx;
}

std::string CitiesManager::findNearest(float lat, float lon,
                                       float *dist_miles) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!initialized_)
    return "";

  float best_d2 = 1e10f;
  const char *best_name = nullptr;

  int lat_idx_center = static_cast<int>(lat + 90.0f);
  int lon_idx_center = static_cast<int>(lon + 180.0f);

  // Search a 3x3 grid of cells around the target location to ensure we find the
  // nearest city even if it's across a cell boundary.
  for (int lat_offset = -1; lat_offset <= 1; ++lat_offset) {
    for (int lon_offset = -1; lon_offset <= 1; ++lon_offset) {
      int lat_idx = (lat_idx_center + lat_offset + GRID_LAT_SIZE) % GRID_LAT_SIZE;
      int lon_idx = (lon_idx_center + lon_offset + GRID_LON_SIZE) % GRID_LON_SIZE;

      const auto& cell_indices = grid_[lat_idx][lon_idx];
      for (size_t city_idx : cell_indices) {
        const auto& city = g_CityData[city_idx];
        float d2 = get_dist_sq(lat, lon, city.lat, city.lon);
        if (d2 < best_d2) {
          best_d2 = d2;
          best_name = city.name;
        }
      }
    }
  }

  // As a fallback for sparse areas, check a wider area if no city was found within ~1 degree.
  if (best_name && best_d2 > 1.0) {
      for (int lat_offset = -2; lat_offset <= 2; ++lat_offset) {
        for (int lon_offset = -2; lon_offset <= 2; ++lon_offset) {
          if (abs(lat_offset) <=1 && abs(lon_offset) <=1) continue; // skip already checked cells
          int lat_idx = (lat_idx_center + lat_offset + GRID_LAT_SIZE) % GRID_LAT_SIZE;
          int lon_idx = (lon_idx_center + lon_offset + GRID_LON_SIZE) % GRID_LON_SIZE;

          const auto& cell_indices = grid_[lat_idx][lon_idx];
          for (size_t city_idx : cell_indices) {
            const auto& city = g_CityData[city_idx];
            float d2 = get_dist_sq(lat, lon, city.lat, city.lon);
            if (d2 < best_d2) {
              best_d2 = d2;
              best_name = city.name;
            }
          }
        }
      }
  }


  if (best_name) {
    // Only return a city if it's within a reasonable distance (e.g., ~2.5 degrees, approx 175 miles)
    if (best_d2 > (2.5 * 2.5)) {
        return "";
    }
    if (dist_miles) {
      // Very rough conversion: 1 degree is ~69.1 miles
      *dist_miles = std::sqrt(best_d2) * 69.1f;
    }
    return best_name;
  }
  return "";
}
