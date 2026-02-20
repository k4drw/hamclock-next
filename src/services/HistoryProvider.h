#pragma once

#include "../core/HistoryData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class HistoryProvider {
public:
  enum class SeriesType { Flux, SSN, Kp };

  HistoryProvider(NetworkManager &net, std::shared_ptr<HistoryStore> store);

  void fetchFlux();
  void fetchSSN();
  void fetchKp();

private:
  NetworkManager &net_;
  std::shared_ptr<HistoryStore> store_;

  static constexpr const char *FLUX_URL =
      "https://services.swpc.noaa.gov/text/daily-solar-indices.txt";
  static constexpr const char *KP_URL =
      "https://services.swpc.noaa.gov/text/daily-geomagnetic-indices.txt";
};
