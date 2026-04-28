#include "HeardMeStore.h"

void HeardMeStore::addSpot(const DXClusterSpot &spot) {
  std::lock_guard<std::mutex> lock(mutex_);
  spots_.push_front(spot);
  if (spots_.size() > MAX_SPOTS) {
    spots_.pop_back();
  }
}

std::vector<DXClusterSpot> HeardMeStore::getSpots() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::vector<DXClusterSpot>(spots_.begin(), spots_.end());
}

void HeardMeStore::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  spots_.clear();
}
