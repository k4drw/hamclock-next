#pragma once

#include "DXClusterData.h"
#include <deque>
#include <mutex>
#include <vector>

// Store for RBN spots where the user is the one being heard.
// Keeps a fixed-size ring buffer of the last 20 hits.
class HeardMeStore {
public:
  HeardMeStore() = default;

  // Add a new spot. If we have 20, the oldest is dropped.
  void addSpot(const DXClusterSpot &spot);

  // Return a copy of all stored spots.
  std::vector<DXClusterSpot> getSpots() const;

  // Clear all spots.
  void clear();

private:
  mutable std::mutex mutex_;
  std::deque<DXClusterSpot> spots_;
  static constexpr size_t MAX_SPOTS = 20;
};
