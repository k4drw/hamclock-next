#include "AsteroidProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include <algorithm>
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
  url << API_BASE_URL << "?dist-max=5000000km&date-min=now&sort=date";

  LOG_I("AsteroidProvider", "Fetching key-less NEO data from JPL (max 5M km)");

  netMgr_.fetchAsync(url.str(), [this](std::string body) {
    if (body.empty()) {
      LOG_E("AsteroidProvider", "Empty response from JPL API");
      isFetching_ = false;
      return;
    }
    processResponse(body);
    isFetching_ = false;
    lastUpdate_ = std::chrono::system_clock::now();
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
