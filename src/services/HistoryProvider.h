#pragma once

#include "../core/HistoryData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class HistoryProvider {
public:
  HistoryProvider(NetworkManager &net, std::shared_ptr<HistoryStore> store);

  void fetchFlux();
  void fetchSSN();
  void fetchKp();

private:
  void processFlux(const std::string &body);
  void processSSN(const std::string &body);
  void processKp(const std::string &body);

  NetworkManager &net_;
  std::shared_ptr<HistoryStore> store_;

  static constexpr const char *FLUX_URL =
      "https://services.swpc.noaa.gov/text/daily-solar-indices.txt";
  static constexpr const char *KP_URL =
      "https://services.swpc.noaa.gov/text/daily-geomagnetic-indices.txt";
};
