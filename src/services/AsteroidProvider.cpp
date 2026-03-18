#include "AsteroidProvider.h"
#include "../core/Constants.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <nlohmann/json.hpp>
#include <sstream>

// JPL SSD CAD API (Key-less, unrestricted scientific access)
static const char *API_BASE_URL = "https://ssd-api.jpl.nasa.gov/cad.api";

AsteroidProvider::AsteroidProvider(NetworkManager &netMgr) : netMgr_(netMgr) {}

std::string AsteroidProvider::getCurrentDate() const {
  auto now = std::chrono::system_clock::now();
  std::time_t t_now = std::chrono::system_clock::to_time_t(now);
  struct tm *tm_now = std::localtime(&t_now);
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm_now);
  return std::string(buf);
}

void AsteroidProvider::update(bool force) {
  lastFetchMs_ = SDL_GetTicks();
  // Always filter stale asteroids from local cache, even between fetches
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now_clock = std::chrono::system_clock::now();
    std::time_t t_now = std::chrono::system_clock::to_time_t(now_clock);
    double currentJD = (static_cast<double>(t_now) / 86400.0) + 2440587.5;
    double gracePeriod = 30.0 / (24.0 * 60.0); // 30 mins grace

    auto it = std::remove_if(
        cachedData_.asteroids.begin(), cachedData_.asteroids.end(),
        [currentJD, gracePeriod](const Asteroid &ast) {
          return ast.julianDate < (currentJD - gracePeriod);
        });
    cachedData_.asteroids.erase(it, cachedData_.asteroids.end());
  }

  if (isFetching_)
    return;

  auto now = std::chrono::system_clock::now();
  // Fetch new data every hour (JPL updates regularly)
  if (!force && cachedData_.valid &&
      (now - lastUpdate_ < std::chrono::hours(1))) {
    return;
  }

  fetchInternal();
}

void AsteroidProvider::fetchInternal() {
  isFetching_ = true;

  // Query: Max 5M km, starting now, sorted by date
  std::stringstream url;
  url << API_BASE_URL << "?dist-max=8000000km&date-min=now&sort=date";

  LOG_I("AsteroidProvider", "Fetching key-less NEO data from JPL (max 8M km)");

  std::weak_ptr<AsteroidProvider> self = shared_from_this();
  netMgr_.fetchAsync(url.str(), [self](std::string body) {
    auto p = self.lock();
    if (!p)
      return;
    if (body.empty()) {
      LOG_E("AsteroidProvider", "Empty response from JPL API");
      p->isFetching_ = false;
      return;
    }
    p->processResponse(body);
    p->isFetching_ = false;
    p->lastUpdate_ = std::chrono::system_clock::now();
  });
}

void AsteroidProvider::processResponse(const std::string &body) {
  std::lock_guard<std::mutex> lock(mutex_);
  try {
    auto j = nlohmann::json::parse(body);

    if (!j.contains("data") || !j.contains("fields")) {
      LOG_E("AsteroidProvider", "Invalid JPL JSON format: missing fields/data");
      return;
    }

    cachedData_.asteroids.clear();
    auto fields = j["fields"];
    auto data = j["data"];

    // Find column indices
    int iName = -1, iDate = -1, iDist = -1, iVRel = -1, iH = -1, iJD = -1;
    for (size_t i = 0; i < fields.size(); ++i) {
      std::string f = fields[i];
      if (f == "des")
        iName = i;
      else if (f == "cd")
        iDate = i;
      else if (f == "dist")
        iDist = i;
      else if (f == "v_rel")
        iVRel = i;
      else if (f == "h")
        iH = i;
      else if (f == "jd")
        iJD = i;
    }

    if (iName == -1 || iDate == -1 || iDist == -1 || iJD == -1) {
      LOG_E("AsteroidProvider", "Missing required columns in JPL response");
      return;
    }

    for (const auto &row : data) {
      if (row.size() <=
          static_cast<size_t>(std::max({iName, iDate, iDist, iVRel, iH, iJD})))
        continue;

      Asteroid ast;
      ast.name = row[iName].get<std::string>();
      ast.julianDate = StringUtils::safe_stod(row[iJD].get<std::string>());

      // JPL format: "2026-Feb-19 11:20"
      std::string rawDate = row[iDate].get<std::string>();
      if (rawDate.length() >= 10) {
        ast.approachDate = rawDate.substr(0, 11); // "2026-Feb-19"
        if (rawDate.length() >= 16) {
          ast.closeApproachTime = rawDate.substr(12, 5);
        }
      }

      ast.missDistanceKm =
          StringUtils::safe_stod(row[iDist].get<std::string>()) *
          149597870.7; // AU to KM
      ast.missDistanceLD = ast.missDistanceKm / 384400.0;

      if (iVRel != -1) {
        ast.velocityKmS = StringUtils::safe_stod(row[iVRel].get<std::string>());
      }

      if (iH != -1) {
        ast.absoluteMagnitude =
            StringUtils::safe_stod(row[iH].get<std::string>());
      }

      // Logic: If H < 22 and miss distance is small, it's potentially hazardous
      ast.isHazardous =
          (ast.absoluteMagnitude <= 22.0 && ast.missDistanceLD < 19.5);

      cachedData_.asteroids.push_back(ast);
    }

    // Sort by Julian Date (reliable chronological sort)
    std::sort(cachedData_.asteroids.begin(), cachedData_.asteroids.end(),
              [](const Asteroid &a, const Asteroid &b) {
                return a.julianDate < b.julianDate;
              });

    cachedData_.valid = true;
    cachedData_.lastFetchTime = getCurrentDate();
    LOG_I("AsteroidProvider", "Fetched {} prospective NEOs from JPL",
          cachedData_.asteroids.size());

  } catch (const std::exception &e) {
    LOG_E("AsteroidProvider", "JSON parse error: {}", e.what());
  }
}

AsteroidData AsteroidProvider::getLatest() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return cachedData_;
}

std::string AsteroidProvider::urlEncode(const std::string &s) {
  std::string out;
  out.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += static_cast<char>(c);
    } else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

void AsteroidProvider::fetchOrbitalElements(const std::string &des) {
#ifdef __EMSCRIPTEN__
  (void)des;
  return; // SBDB fetch not available in WASM
#else
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (orbitalElementsCache_.count(des))
      return; // already cached
  }

  std::string url = std::string("https://ssd-api.jpl.nasa.gov/sbdb.api?sstr=") +
                    urlEncode(des);

  LOG_I("AsteroidProvider", "Fetching orbital elements for '{}'", des);

  std::weak_ptr<AsteroidProvider> self = shared_from_this();
  netMgr_.fetchAsync(url, [self, des](std::string body) {
    auto p = self.lock();
    if (!p)
      return;
    if (body.empty()) {
      LOG_E("AsteroidProvider", "Empty SBDB response for '{}'", des);
      return;
    }
    p->processElementsResponse(des, body);
  });
#endif
}

void AsteroidProvider::processElementsResponse(const std::string &des,
                                               const std::string &body) {
  OrbitalElements elem;
  try {
    auto j = nlohmann::json::parse(body);

    if (!j.contains("orbit") || !j["orbit"].contains("elements") ||
        !j["orbit"].contains("epoch")) {
      LOG_E("AsteroidProvider", "SBDB response missing orbit data for '{}'",
            des);
      return;
    }

    elem.epoch_jd =
        StringUtils::safe_stod(j["orbit"]["epoch"].get<std::string>());

    for (const auto &el : j["orbit"]["elements"]) {
      std::string label = el["label"].get<std::string>();
      double val = StringUtils::safe_stod(el["value"].get<std::string>());
      if (label == "e")
        elem.e = val;
      else if (label == "a")
        elem.a = val;
      else if (label == "i")
        elem.i = val;
      else if (label == "om")
        elem.om = val;
      else if (label == "w")
        elem.w = val;
      else if (label == "ma")
        elem.ma = val;
    }

    elem.valid = (elem.a > 0 && elem.epoch_jd > 0);

    LOG_I("AsteroidProvider",
          "Got orbital elements for '{}': a={:.4f} e={:.4f}", des, elem.a,
          elem.e);
  } catch (const std::exception &e) {
    LOG_E("AsteroidProvider", "SBDB parse error for '{}': {}", des, e.what());
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    orbitalElementsCache_[des] = elem;
  }

  // Notify map widget via SDL event
  auto *payload = new std::pair<std::string, OrbitalElements>(des, elem);
  SDL_Event ev;
  SDL_zero(ev);
  ev.type = HamClock::AE_BASE_EVENT + HamClock::AE_ASTEROID_ELEMENTS_READY;
  ev.user.data1 = payload;
  SDL_PushEvent(&ev);
}

OrbitalElements
AsteroidProvider::getOrbitalElements(const std::string &des) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = orbitalElementsCache_.find(des);
  if (it != orbitalElementsCache_.end())
    return it->second;
  return OrbitalElements{};
}
