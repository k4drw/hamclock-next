#include "IonosondeProvider.h"
#include "../core/Astronomy.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

IonosondeProvider::IonosondeProvider(NetworkManager &netMgr)
    : netMgr_(netMgr) {}

void IonosondeProvider::update() {
  uint32_t now = SDL_GetTicks();
  if (hasData_ && (now - lastUpdateMs_ < UPDATE_INTERVAL_MS)) {
    return;
  }

  const char *url = "https://prop.kc2g.com/api/stations.json";
  LOG_I("IonosondeProvider", "Fetching ionosonde data from {}", url);

  netMgr_.fetchAsync(url, [this, now](std::string body) {
    if (!body.empty()) {
      processData(body);
      lastUpdateMs_ = now;
    } else {
      LOG_E("IonosondeProvider", "Failed to fetch ionosonde data");
    }
  });
}

void IonosondeProvider::processData(const std::string &body) {
  try {
    auto j = json::parse(body);
    std::vector<IonosondeStation> newStations;

    for (auto &s : j) {
      if (!s.contains("fof2") || !s.contains("station"))
        continue;

      int cs = s.value("cs", 0);
      if (cs <= 0)
        continue;

      IonosondeStation station;
      station.code = s["station"].value("code", "");
      station.name = s["station"].value("name", "");
      auto safeGetLatLon = [](const json &j, const char *key) -> double {
        if (!j.contains(key) || j[key].is_null())
          return 0.0;
        if (j[key].is_number())
          return j[key].get<double>();
        if (j[key].is_string())
          return StringUtils::safe_stod(j[key].get<std::string>());
        return 0.0;
      };

      station.lat = safeGetLatLon(s["station"], "latitude");
      station.lon = safeGetLatLon(s["station"], "longitude");
      if (station.lon > 180.0)
        station.lon -= 360.0;

      auto safeGetDouble = [](const json &j,
                              const char *key) -> std::optional<double> {
        if (!j.contains(key) || j[key].is_null())
          return std::nullopt;
        if (j[key].is_number())
          return j[key].get<double>();
        if (j[key].is_string()) {
          double v = StringUtils::safe_stod(j[key].get<std::string>());
          if (v != 0.0 || !j[key].get<std::string>().empty())
            return v;
        }
        return std::nullopt;
      };

      if (auto val = safeGetDouble(s, "fof2")) {
        station.foF2 = *val;
      } else {
        // Essential field missing, skip station
        continue;
      }

      if (auto val = safeGetDouble(s, "mufd")) {
        station.mufd = *val;
      }
      if (auto val = safeGetDouble(s, "hmf2")) {
        station.hmF2 = *val;
      }
      if (auto val = safeGetDouble(s, "md")) {
        station.md = *val;
      } else {
        station.md = 3.0;
      }
      station.confidence = cs;

      // Time check could be added here, but relying on KC2G's fresh stations
      // list is usually enough
      newStations.push_back(station);
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      stations_ = std::move(newStations);
      hasData_ = true;
      LOG_I("IonosondeProvider", "Processed {} valid ionosonde stations",
            stations_.size());
    }
  } catch (const std::exception &e) {
    LOG_E("IonosondeProvider", "Failed to parse ionosonde JSON: {}", e.what());
  }
}

InterpolatedIonosonde IonosondeProvider::interpolate(double lat,
                                                     double lon) const {
  std::lock_guard<std::mutex> lock(mutex_);
  InterpolatedIonosonde result;

  if (stations_.empty())
    return result;

  struct NearStation {
    const IonosondeStation *station;
    double dist;
  };
  std::vector<NearStation> neighbors;

  for (const auto &s : stations_) {
    double d = Astronomy::calculateDistance({lat, lon}, {s.lat, s.lon});
    if (d < result.nearestDistance)
      result.nearestDistance = d;

    if (d <= MAX_VALID_DISTANCE_KM) {
      neighbors.push_back({&s, d});
    }
  }

  if (neighbors.empty())
    return result;

  // Sort by distance and take top 5
  std::sort(neighbors.begin(), neighbors.end(),
            [](const NearStation &a, const NearStation &b) {
              return a.dist < b.dist;
            });
  if (neighbors.size() > 5)
    neighbors.resize(5);

  // Direct match check (within 50km)
  if (neighbors[0].dist < 50.0) {
    const auto *s = neighbors[0].station;
    result.foF2 = s->foF2;
    result.mufd = s->mufd;
    result.hmF2 = s->hmF2;
    result.md = s->md;
    result.stationsUsed = 1;
    return result;
  }

  // IDW Calculation
  auto weightedAvg = [&](auto getVal, auto hasVal) -> std::optional<double> {
    double sumWeights = 0.0;
    double sumVal = 0.0;
    bool any = false;

    for (const auto &n : neighbors) {
      if (hasVal(n.station)) {
        // Weight = (Confidence / 100) / Distance^2
        double w =
            (n.station->confidence / 100.0) / std::max(1.0, n.dist * n.dist);
        sumWeights += w;
        sumVal += getVal(n.station) * w;
        any = true;
      }
    }
    return any ? std::optional<double>(sumVal / sumWeights) : std::nullopt;
  };

  result.foF2 = weightedAvg([](const IonosondeStation *s) { return s->foF2; },
                            [](const IonosondeStation *s) { return true; })
                    .value_or(0.0);

  result.mufd = weightedAvg(
      [](const IonosondeStation *s) { return s->mufd.value(); },
      [](const IonosondeStation *s) { return s->mufd.has_value(); });

  result.hmF2 = weightedAvg(
      [](const IonosondeStation *s) { return s->hmF2.value(); },
      [](const IonosondeStation *s) { return s->hmF2.has_value(); });

  result.md = weightedAvg([](const IonosondeStation *s) { return s->md; },
                          [](const IonosondeStation *s) { return true; })
                  .value_or(3.0);

  result.stationsUsed = static_cast<int>(neighbors.size());

  return result;
}

bool IonosondeProvider::hasData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return hasData_;
}
