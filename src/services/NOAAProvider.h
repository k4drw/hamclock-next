#pragma once

#include "../core/AuroraHistoryStore.h"
#include "../core/AuroraMapData.h"
#include "../core/DRAPData.h"
#include "../core/SolarData.h"
#include "../core/XRayData.h"
#include "../network/NetworkManager.h"

#include <memory>

struct HamClockState;

class NOAAProvider {
public:
  enum class UpdateType {
    KIndex,
    SFI,
    SN,
    Plasma,
    Mag,
    DST,
    Aurora,
    DRAP,
    XRay,
    ProtonFlux
  };

  NOAAProvider(NetworkManager &net, std::shared_ptr<SolarDataStore> store,
               std::shared_ptr<AuroraHistoryStore> auroraStore = nullptr,
               std::shared_ptr<XRayHistoryStore> xrayStore = nullptr,
               HamClockState *state = nullptr);

  void fetch();
  void fetchDRAP();
  void fetchAuroraMap();
  void setDrapStore(std::shared_ptr<DRAPDataStore> s) {
    drapStore_ = std::move(s);
  }
  void setAuroraMapStore(std::shared_ptr<AuroraMapStore> s) {
    auroraMapStore_ = std::move(s);
  }

private:
  void fetchKIndex();
  void fetchSFI();
  void fetchSN();
  void fetchPlasma();
  void fetchMag();
  void fetchDST();
  void fetchAurora();
  void fetchAuroraHistory();
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
  // 7-day versions used for aurora history backfill
  static constexpr const char *PLASMA_7D_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/plasma-7-day.json";
  static constexpr const char *MAG_7D_URL =
      "https://services.swpc.noaa.gov/products/solar-wind/mag-7-day.json";
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
  std::shared_ptr<XRayHistoryStore> xrayStore_;
  std::shared_ptr<DRAPDataStore> drapStore_;
  std::shared_ptr<AuroraMapStore> auroraMapStore_;
  HamClockState *state_;
};
