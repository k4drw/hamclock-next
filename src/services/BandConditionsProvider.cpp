#include "BandConditionsProvider.h"
#include <chrono>

BandConditionsProvider::BandConditionsProvider(
    std::shared_ptr<SolarDataStore> solarStore,
    std::shared_ptr<BandConditionsStore> bandStore)
    : solarStore_(std::move(solarStore)), bandStore_(std::move(bandStore)) {}

void BandConditionsProvider::update() {
  SolarData solar = solarStore_->get();
  if (!solar.valid)
    return;

  BandConditionsData data;
  // Common bands for propagation display
  static const std::vector<std::string> bands = {"80m", "40m", "20m", "15m",
                                                 "10m"};

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

  return BandCondition::UNKNOWN;
}
