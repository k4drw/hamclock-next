#pragma once

#include "../core/SolarData.h"
#include "../network/NetworkManager.h"

#include <memory>

class NOAAProvider {
public:
  NOAAProvider(NetworkManager &net, std::shared_ptr<SolarDataStore> store);

  void fetch();

private:
  void fetchKIndex();
  void fetchSFI();
  void fetchSN();
  void fetchPlasma();
  void fetchMag();
  void fetchDST();
  void fetchAurora();

  static constexpr const char *K_INDEX_URL =
      "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json";
  static constexpr const char *SFI_URL =
      "https://services.swpc.noaa.gov/products/summary/10cm-flux.json";
  static constexpr const char *SN_URL =
      "https://services.swpc.noaa.gov/products/noaa-sunspot-number.json";
  static constexpr const char *PLASMA_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/plasma-5-minute.json";
  static constexpr const char *MAG_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/mag-5-minute.json";
  static constexpr const char *DST_URL =
      "https://services.swpc.noaa.gov/products/kyoto-dst.json";
  static constexpr const char *AURORA_URL =
      "https://services.swpc.noaa.gov/text/aurora-nowcast-hemi-power.txt";

  NetworkManager &net_;
  std::shared_ptr<SolarDataStore> store_;
};
