#pragma once

#include "../core/AuroraHistoryStore.h"
#include "../core/SolarData.h"
#include "../network/NetworkManager.h"

#include <memory>

struct HamClockState;

class NOAAProvider {
public:
  NOAAProvider(NetworkManager &net, std::shared_ptr<SolarDataStore> store,
               std::shared_ptr<AuroraHistoryStore> auroraStore = nullptr,
               HamClockState *state = nullptr);

  void fetch();

private:
  void fetchKIndex();
  void fetchSFI();
  void fetchSN();
  void fetchPlasma();
  void fetchMag();
  void fetchDST();
  void fetchAurora();
  void fetchDRAP();
  void fetchXRay();
  void fetchProtonFlux();

  static constexpr const char *K_INDEX_URL =
      "https://services.swpc.noaa.gov/products/noaa-planetary-k-index.json";
  static constexpr const char *SFI_URL =
      "https://services.swpc.noaa.gov/products/summary/10cm-flux.json";
  static constexpr const char *SN_URL =
      "https://services.swpc.noaa.gov/json/solar-cycle/"
      "predicted-solar-cycle.json";
  static constexpr const char *PLASMA_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/plasma-5-minute.json";
  static constexpr const char *MAG_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/mag-5-minute.json";
  static constexpr const char *DST_URL =
      "https://services.swpc.noaa.gov/products/kyoto-dst.json";
  static constexpr const char *AURORA_URL =
      "https://services.swpc.noaa.gov/json/ovation_aurora_latest.json";
  static constexpr const char *DRAP_URL =
      "https://services.swpc.noaa.gov/text/drap_global_frequencies.txt";
  static constexpr const char *XRAY_URL =
      "https://services.swpc.noaa.gov/json/goes/primary/xrays-6-hour.json";
  static constexpr const char *PROTON_URL =
      "https://services.swpc.noaa.gov/json/goes/primary/"
      "integral-protons-6-hour.json";

  NetworkManager &net_;
  std::shared_ptr<SolarDataStore> store_;
  std::shared_ptr<AuroraHistoryStore> auroraStore_;
  HamClockState *state_;
};
