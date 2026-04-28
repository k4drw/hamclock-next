#include "BandConditionsProvider.h"
#include "../core/Logger.h"
#include <chrono>
#include <SDL.h>

BandConditionsProvider::BandConditionsProvider(
    std::shared_ptr<SolarDataStore> solarStore,
    std::shared_ptr<BandConditionsStore> bandStore)
    : solarStore_(std::move(solarStore)), bandStore_(std::move(bandStore)) {}

void BandConditionsProvider::update() {
  lastFetchMs_ = SDL_GetTicks();
  SolarData solar = solarStore_->get();
  if (!solar.valid) {
    LOG_D("BandConditions", "Solar data not yet valid, skipping update");
    return;
  }
  LOG_D("BandConditions", "Updating band conditions: SFI={}, K={}", solar.sfi, solar.k_index);

  BandConditionsData data;
  // Common bands for propagation display
  static const std::vector<std::string> bands = {"80m", "40m", "20m", "15m",
                                                 "10m", "6m"};

  // Store the solar data used for calculations
  data.sfi = solar.sfi;
  data.k_index = solar.k_index;

  for (const auto &b : bands) {
    BandStatus status;
    status.band = b;
    status.day = calculate(solar.sfi, solar.k_index, b, true);
    status.night = calculate(solar.sfi, solar.k_index, b, false);
    data.statuses.push_back(status);
  }

  data.lastUpdate = std::chrono::system_clock::now();
  data.valid = true;
  bandStore_->update(data);
}

BandCondition BandConditionsProvider::calculate(int sfi, int k,
                                                const std::string &band,
                                                bool day) {
  // Simplified propagation model based on SFI and K-index
  // Sources: various amateur radio propagation charts (N0NBH, etc.)

  if (band == "80m") {
    if (day)
      return BandCondition::POOR;
    if (k >= 5)
      return BandCondition::POOR;
    if (k >= 3)
      return BandCondition::FAIR;
    return BandCondition::GOOD;
  }

  if (band == "40m") {
    if (day) {
      if (sfi > 150)
        return BandCondition::FAIR;
      return BandCondition::POOR;
    }
    // Night
    if (k >= 5)
      return BandCondition::POOR;
    if (k >= 3)
      return BandCondition::FAIR;
    if (sfi > 100)
      return BandCondition::EXCELLENT;
    return BandCondition::GOOD;
  }

  if (band == "20m") {
    if (k >= 5)
      return BandCondition::POOR;
    if (day) {
      if (sfi > 150)
        return BandCondition::EXCELLENT;
      if (sfi > 100)
        return BandCondition::GOOD;
      if (sfi > 70)
        return BandCondition::FAIR;
      return BandCondition::POOR;
    } else {
      // Night
      if (sfi > 120)
        return BandCondition::GOOD;
      if (sfi > 90)
        return BandCondition::FAIR;
      return BandCondition::POOR;
    }
  }

  if (band == "15m") {
    if (!day)
      return BandCondition::POOR;
    if (k >= 4)
      return BandCondition::POOR;
    if (sfi > 180)
      return BandCondition::EXCELLENT;
    if (sfi > 120)
      return BandCondition::GOOD;
    if (sfi > 90)
      return BandCondition::FAIR;
    return BandCondition::POOR;
  }

  if (band == "10m") {
    if (!day)
      return BandCondition::POOR;
    if (k >= 4)
      return BandCondition::POOR;
    if (sfi > 250)
      return BandCondition::EXCELLENT;
    if (sfi > 180)
      return BandCondition::GOOD;
    if (sfi > 140)
      return BandCondition::FAIR;
    return BandCondition::POOR;
  }

  if (band == "6m") {
    if (!day)
      return BandCondition::POOR;
    if (k >= 5)
      return BandCondition::POOR;
    if (sfi > 200)
      return BandCondition::EXCELLENT;
    if (sfi > 150)
      return BandCondition::GOOD;
    if (sfi > 100)
      return BandCondition::FAIR;
    return BandCondition::POOR;
  }

  return BandCondition::UNKNOWN;
}
