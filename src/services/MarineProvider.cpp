#include "MarineProvider.h"
#include "../core/Constants.h"
#include "../core/StringUtils.h"
#include "../core/WorkerService.h"
#include "../core/Astronomy.h"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <sstream>

#include <nlohmann/json.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;


MarineProvider::MarineProvider(NetworkManager &net,
                               std::shared_ptr<MarineStore> store)
    : net_(net), store_(std::move(store)) {}

void MarineProvider::fetch(const std::string &tideStation,
                           const std::string &buoyStation, bool force) {
  lastFetchMs_ = SDL_GetTicks();
  if (tideStation.empty() && buoyStation.empty())
    return;

  auto store = store_;

  if (!tideStation.empty()) {
    // Check if we need to fetch name metadata
    auto current = store_->get();
    if (current.tideStationId != tideStation || current.tideStationName.empty()) {
      fetchStationName(tideStation);
    }

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
              MarineData update; 
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
                    tp.heightFt = StringUtils::safe_stod(p["v"].get<std::string>());
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
  std::string stId = buoyStation;
  // NDBC URLs are case-sensitive; force uppercase IDs.
  std::string upperId = stId;
  std::transform(upperId.begin(), upperId.end(), upperId.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  char url[256];
  std::snprintf(url, sizeof(url),
                "https://www.ndbc.noaa.gov/data/realtime2/%s.txt",
                upperId.c_str());

  auto store = store_;
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

            MarineData update;
            BuoyObservation &b = update.buoy;
            b.stationId = stId;

            auto toDouble = [](const std::string &s, double missing) -> double {
              if (s == "MM" || s == "9999" || s == "999.0")
                return missing;
              return StringUtils::safe_stod(s);
            };

            b.waveHeightM = toDouble(colVal("WVHT"), -1.0);
            b.wavePeriodS = toDouble(colVal("DPD"), -1.0);
            b.waterTempC = toDouble(colVal("WTMP"), -999.0);
            b.windSpeedMps = toDouble(colVal("WSPD"), -1.0);
            auto wdStr = colVal("WDIR");
            if (wdStr != "MM")
              b.windDirDeg = StringUtils::safe_stoi(wdStr);

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

void MarineProvider::fetchStationName(const std::string &stId) {
  char url[256];
  // NOAA CO-OPS Station Metadata API
  std::snprintf(
      url, sizeof(url),
      "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/stations/%s.json",
      stId.c_str());

  auto store = store_;
  std::string targetId = stId;
  net_.fetchAsync(url, [store, targetId](std::string body) {
    if (body.empty())
      return;
    try {
      auto j = json::parse(body);
      if (j.contains("stations") && j["stations"].is_array() &&
          !j["stations"].empty()) {
        const auto &s = j["stations"][0];
        std::string name = s.value("name", "");
        if (!name.empty()) {
          std::string state = s.value("state", "");
          if (!state.empty()) {
            name += ", " + state;
          }
          auto current = store->get();

          // Verify ID still matches to prevent race
          if (current.tideStationId == targetId || current.tideStationId.empty()) {
            current.tideStationName = name;
            store->update(current);
          }
        }
      }
    } catch (...) {
    }
  });
}


void MarineProvider::lookupClosestStations(double lat, double lon) {
  // NOAA Tide Stations (JSON)
  std::string tidesUrl =
      "https://api.tidesandcurrents.noaa.gov/mdapi/prod/webapi/"
      "stations.json?features=predict";

  net_.fetchAsync(tidesUrl, [this, lat, lon](std::string tidesBody) {
    if (tidesBody.empty())
      return;

    // NDBC Active Stations (XML)
    std::string buoysUrl = "https://www.ndbc.noaa.gov/activestations.xml";
    net_.fetchAsync(buoysUrl, [lat, lon, tidesBody](std::string buoysBody) {
      if (buoysBody.empty())
        return;

      WorkerService::getInstance().submitTask([lat, lon, tidesBody, buoysBody]() {
        MarineLookupResult result;
        LatLon home = {lat, lon};

        // --- Parse Tides (JSON) ---
        try {
          auto j = json::parse(tidesBody);
          if (j.contains("stations") && j["stations"].is_array()) {
            double minTideDist = 1e10;
            for (const auto &s : j["stations"]) {
              if (s.contains("id") && s.contains("lat") && s.contains("lng")) {
                LatLon stLoc = {s["lat"].get<double>(), s["lng"].get<double>()};
                double d = Astronomy::calculateDistance(home, stLoc);
                if (d < minTideDist) {
                  minTideDist = d;
                  result.closestTideId = s["id"].get<std::string>();
                  result.closestTideName = s.value("name", "");
                  result.distKmTide = d;
                }
              }
            }
          }
        } catch (...) {
        }

        // --- Parse Buoys (XML) ---
        // High-performance stream-based parsing for <station id="..." lat="..." lon="..." />
        std::istringstream ss(buoysBody);
        std::string line;
        double minBuoyDist = 1e10;
        while (std::getline(ss, line)) {
          if (line.find("<station") == std::string::npos)
            continue;

          auto getAttr = [&](const std::string &attr) -> std::string {
            std::string search = attr + "=\"";
            size_t start = line.find(search);
            if (start == std::string::npos)
              return "";
            start += search.size();
            size_t end = line.find("\"", start);
            if (end == std::string::npos)
              return "";
            return line.substr(start, end - start);
          };

          std::string id = getAttr("id");
          std::string sLat = getAttr("lat");
          std::string sLon = getAttr("lon");
          std::string name = getAttr("name");

          if (!id.empty() && !sLat.empty() && !sLon.empty()) {
            try {
              LatLon stLoc = {StringUtils::safe_stod(sLat),
                              StringUtils::safe_stod(sLon)};
              double d = Astronomy::calculateDistance(home, stLoc);
              if (d < minBuoyDist) {
                minBuoyDist = d;
                result.closestBuoyId = id;
                result.closestBuoyName = name;
                result.distKmBuoy = d;
              }
            } catch (...) {
            }
          }
        }

        // --- Deliver results ---
        SDL_Event event;
        SDL_zero(event);
        event.type =
            HamClock::AE_BASE_EVENT + HamClock::AE_MARINE_LOOKUP_READY;
        event.user.data1 = new MarineLookupResult(result);
        SDL_PushEvent(&event);
      });
    });
  });
}
