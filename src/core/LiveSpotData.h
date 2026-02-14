#pragma once

#include <chrono>
#include <cstring>
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
  LiveSpotData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  // Set provider data, preserving UI-driven selectedBands state.
  void set(const LiveSpotData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool saved[kNumBands];
    std::memcpy(saved, data_.selectedBands, sizeof(saved));
    data_ = data;
    std::memcpy(data_.selectedBands, saved, sizeof(saved));
  }

  void toggleBand(int idx) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (idx >= 0 && idx < kNumBands)
      data_.selectedBands[idx] = !data_.selectedBands[idx];
  }

private:
  mutable std::mutex mutex_;
  LiveSpotData data_;
};
