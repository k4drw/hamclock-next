#pragma once

#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <SDL.h>

// Ham radio band definitions for spot aggregation.
struct BandDef {
  const char *name;
  double minKhz;
  double maxKhz;
  SDL_Color color;
};

static constexpr int kNumBands = 12;

// Band colors match original HamClock palette.
static const BandDef kBands[kNumBands] = {
    {"160m", 1800, 2000, {200, 0, 0, 255}},
    {"80m", 3500, 4000, {255, 128, 0, 255}},
    {"60m", 5330, 5410, {128, 128, 0, 255}},
    {"40m", 7000, 7300, {0, 200, 0, 255}},
    {"30m", 10100, 10150, {0, 128, 128, 255}},
    {"20m", 14000, 14350, {0, 80, 255, 255}},
    {"17m", 18068, 18168, {30, 144, 255, 255}},
    {"15m", 21000, 21450, {148, 0, 211, 255}},
    {"12m", 24890, 24990, {255, 0, 160, 255}},
    {"10m", 28000, 29700, {160, 82, 45, 255}},
    {"6m", 50000, 54000, {128, 128, 128, 255}},
    {"2m", 144000, 148000, {80, 80, 80, 255}},
};

// Map a frequency (in kHz) to a band index, or -1 if not matched.
inline int freqToBandIndex(double freqKhz) {
  for (int i = 0; i < kNumBands; ++i) {
    if (freqKhz >= kBands[i].minKhz && freqKhz <= kBands[i].maxKhz)
      return i;
  }
  return -1;
}

// Individual spot record for map plotting.
struct SpotRecord {
  double freqKhz;
  std::string receiverGrid; // Maidenhead locator of receiving station
  std::string senderCallsign;
  std::chrono::system_clock::time_point timestamp;
};

struct LiveSpotData {
  int bandCounts[kNumBands] = {};
  std::vector<SpotRecord> spots; // individual spots for map overlay
  bool selectedBands[kNumBands] =
      {};           // which bands are toggled on for map display
  std::string grid; // queried grid square, e.g. "EL87"
  int windowMinutes = 30;
  std::chrono::system_clock::time_point lastUpdated{};
  bool valid = false;
};

class LiveSpotDataStore {
public:
  LiveSpotDataStore() : data_(std::make_shared<LiveSpotData>()) {}

  std::shared_ptr<const LiveSpotData> snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  // Set provider data, preserving UI-driven selectedBands state.
  void set(const LiveSpotData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<LiveSpotData>(data);
    // Preserve UI state
    std::memcpy(newData->selectedBands, data_->selectedBands,
                sizeof(newData->selectedBands));
    data_ = newData;
  }

  void setSelectedBandsMask(uint32_t mask) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<LiveSpotData>(*data_);
    for (int i = 0; i < kNumBands; ++i) {
      newData->selectedBands[i] = (mask & (1 << i)) != 0;
    }
    data_ = newData;
  }

  uint32_t getSelectedBandsMask() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t mask = 0;
    for (int i = 0; i < kNumBands; ++i) {
      if (data_->selectedBands[i])
        mask |= (1 << i);
    }
    return mask;
  }

  void toggleBand(int idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx >= 0 && idx < kNumBands) {
      auto newData = std::make_shared<LiveSpotData>(*data_);
      newData->selectedBands[idx] = !newData->selectedBands[idx];
      data_ = newData;
    }
  }

  void addSpot(const SpotRecord &spot) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<LiveSpotData>(*data_);
    int idx = freqToBandIndex(spot.freqKhz);
    if (idx >= 0) {
      newData->bandCounts[idx]++;
    }
    newData->spots.push_back(spot);
    newData->lastUpdated = std::chrono::system_clock::now();
    newData->valid = true;
    data_ = newData;
  }

  void prune(std::chrono::system_clock::time_point oldest) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<LiveSpotData>(*data_);
    bool changed = false;

    auto it = newData->spots.begin();
    while (it != newData->spots.end()) {
      if (it->timestamp < oldest) {
        int idx = freqToBandIndex(it->freqKhz);
        if (idx >= 0 && newData->bandCounts[idx] > 0) {
          newData->bandCounts[idx]--;
        }
        it = newData->spots.erase(it);
        changed = true;
      } else {
        ++it;
      }
    }

    if (changed) {
      newData->lastUpdated = std::chrono::system_clock::now();
      data_ = newData;
    }
  }

  void clearSpots() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto newData = std::make_shared<LiveSpotData>(*data_);
    newData->spots.clear();
    std::memset(newData->bandCounts, 0, sizeof(newData->bandCounts));
    data_ = newData;
  }

private:
  mutable std::mutex mutex_;
  std::shared_ptr<LiveSpotData> data_;
};
