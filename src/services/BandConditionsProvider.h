#pragma once

#include "../core/BandConditionsData.h"
#include "../core/SolarData.h"
#include <memory>

class BandConditionsProvider {
public:
  BandConditionsProvider(std::shared_ptr<SolarDataStore> solarStore,
                         std::shared_ptr<BandConditionsStore> bandStore);

  void update();

private:
  std::shared_ptr<SolarDataStore> solarStore_;
  std::shared_ptr<BandConditionsStore> bandStore_;

  BandCondition calculate(int sfi, int k, const std::string &band, bool day);
};
