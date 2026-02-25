#include "MarineProvider.h"
#include "../core/Constants.h"
#include "../core/WorkerService.h"
#include <SDL_events.h>
#include <chrono>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <sstream>

using json = nlohmann::json;

MarineProvider::MarineProvider(NetworkManager &net,
                               std::shared_ptr<MarineStore> store)
    : net_(net), store_(std::move(store)) {}

void MarineProvider::fetch(const std::string &tideStation,
                           const std::string &buoyStation, bool force) {
  if (tideStation.empty() && buoyStation.empty())
    return;

  auto store = store_;

  if (!tideStation.empty()) {
    // NOAA CO-OPS tide predictions API: next 2 days, hi/lo intervals.
    char url[512];
    std::snprintf(
        url, sizeof(url),
        "https://api.tidesandcurrents.noaa.gov/api/prod/datagetter"
        "?station=%s&product=predictions&datum=MLLW&time_zone=lst_ldt"
        "&interval=hilo&units=english&application=HamClockNext&format=json"
        "&range=48",
        tideStation.c_str());

    std::string stId = tideStation;
    net_.fetchAsync(
        url,
        [store, stId](std::string body) {
          if (body.empty())
            return;

          WorkerService::getInstance().submitTask([store, body, stId]() {
            try {
              auto j = json::parse(body);
              MarineData update = store->get(); // merge with existing buoy data

              update.tideStationId = stId;
              update.tides.clear();

              if (j.contains("predictions") &&
                  j["predictions"].is_array()) {
                for (const auto &p : j["predictions"]) {
                  TidePrediction tp;
                  if (p.contains("t") && p["t"].is_string()) {
                    std::string ts = p["t"].get<std::string>();
                    // ts format: "2026-02-24 14:30"
                    tp.time = ts.size() >= 16 ? ts.substr(11, 5) : ts;
                  }
                  if (p.contains("type") && p["type"].is_string())
                    tp.type = p["type"].get<std::string>();
                  if (p.contains("v") && p["v"].is_string()) {
                    try {
                      tp.heightFt = std::stod(p["v"].get<std::string>());
                    } catch (...) {
                    }
                  }
                  update.tides.push_back(tp);
                  if ((int)update.tides.size() >= 6)
                    break;
                }
              }

              update.tidesValid = !update.tides.empty();
              update.lastUpdate = std::chrono::system_clock::now();

              SDL_Event event;
              SDL_zero(event);
              event.type =
                  HamClock::AE_BASE_EVENT + HamClock::AE_MARINE_DATA_READY;
              event.user.code = 0;
              event.user.data1 = new MarineData(update);
              SDL_PushEvent(&event);
            } catch (...) {
            }
          });
        },
        3600, force);
  }

  if (!buoyStation.empty())
    fetchBuoy(buoyStation, force);
}

void MarineProvider::fetchBuoy(const std::string &buoyStation, bool force) {
  // NDBC latest observation (text format, space-separated).
  char url[256];
  std::snprintf(url, sizeof(url),
                "https://www.ndbc.noaa.gov/data/latest_obs/%s.txt",
                buoyStation.c_str());

  auto store = store_;
  std::string stId = buoyStation;

  net_.fetchAsync(
      url,
      [store, stId](std::string body) {
        if (body.empty())
          return;

        WorkerService::getInstance().submitTask([store, body, stId]() {
          try {
            // NDBC text: header rows start with '#', data on line after headers
            std::istringstream ss(body);
            std::string line;
            std::vector<std::string> header;
            std::vector<std::string> data;

            while (std::getline(ss, line)) {
              if (line.empty())
                continue;
              if (line[0] == '#') {
                // Parse column names from first header line
                if (header.empty()) {
                  std::istringstream hs(line.substr(1));
                  std::string col;
                  while (hs >> col)
                    header.push_back(col);
                }
                continue;
              }
              // First non-comment line is data
              std::istringstream ds(line);
              std::string val;
              while (ds >> val)
                data.push_back(val);
              break;
            }

            auto colVal = [&](const std::string &name) -> std::string {
              for (size_t i = 0; i < header.size() && i < data.size(); ++i)
                if (header[i] == name)
                  return data[i];
              return "MM";
            };

            MarineData update = store->get();
            BuoyObservation &b = update.buoy;
            b.stationId = stId;

            auto toDouble = [](const std::string &s, double missing) -> double {
              if (s == "MM" || s == "9999" || s == "999.0")
                return missing;
              try {
                return std::stod(s);
              } catch (...) {
                return missing;
              }
            };

            b.waveHeightM = toDouble(colVal("WVHT"), -1.0);
            b.wavePeriodS = toDouble(colVal("DPD"), -1.0);
            b.waterTempC = toDouble(colVal("WTMP"), -999.0);
            b.windSpeedMps = toDouble(colVal("WSPD"), -1.0);
            auto wdStr = colVal("WDIR");
            if (wdStr != "MM")
              try {
                b.windDirDeg = std::stoi(wdStr);
              } catch (...) {
              }

            update.buoyValid = true;
            update.lastUpdate = std::chrono::system_clock::now();

            SDL_Event event;
            SDL_zero(event);
            event.type =
                HamClock::AE_BASE_EVENT + HamClock::AE_MARINE_DATA_READY;
            event.user.code = 1; // code=1 = buoy update
            event.user.data1 = new MarineData(update);
            SDL_PushEvent(&event);
          } catch (...) {
          }
        });
      },
      1800, force);
}
