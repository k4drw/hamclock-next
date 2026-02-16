#include "SatelliteManager.h"
#include "Logger.h"
#include "OrbitPredictor.h"
#include "../services/RotatorService.h"
#include <cmath>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

// Trim whitespace from both ends of a string.
static std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos)
    return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

SatelliteManager::SatelliteManager(NetworkManager &net) : net_(net) {}

void SatelliteManager::fetch(bool force) {
  // Skip if data is fresh (< 24 hours) unless forced
  if (!force && dataValid_) {
    auto elapsed = std::chrono::steady_clock::now() - lastFetch_;
    if (elapsed < std::chrono::hours(24))
      return;
  }

  LOG_I("SatelliteManager", "Fetching TLE data from celestrak...");

  net_.fetchAsync(
      TLE_URL,
      [this](std::string response) {
        if (response.empty()) {
          LOG_E("SatelliteManager", "Fetch failed (empty response)");
          return;
        }
        parse(response);
      },
      86400); // 24 hour cache age
}

void SatelliteManager::update() {
  if (trackedSatName_.empty() || !rotator_) {
    return;
  }

  const SatelliteTLE *tle = findByName(trackedSatName_);
  if (!tle) {
    return;
  }

  OrbitPredictor predictor;
  predictor.setObserver(obsLat_, obsLon_);
  if (!predictor.loadTLE(*tle)) {
    return;
  }

  SatObservation obs = predictor.observe();
  if (obs.elevation < 0) {
    return; // sat is below horizon
  }

  RotatorData currentPos = rotator_->getPosition();
  double az_error = std::abs(obs.azimuth - currentPos.azimuth);
  double el_error = std::abs(obs.elevation - currentPos.elevation);

  if (az_error > 2.0 || el_error > 2.0) {
    rotator_->setPosition(obs.azimuth, obs.elevation);
  }
}

void SatelliteManager::trackSatellite(const std::string &satName) {
  std::lock_guard<std::mutex> lock(mutex_);
  trackedSatName_ = satName;
}

std::string SatelliteManager::getTrackedSatellite() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return trackedSatName_;
}

void SatelliteManager::parse(const std::string &raw) {
  // TLE format: groups of 3 lines
  //   Line 0: Satellite name
  //   Line 1: 1 NNNNN...
  //   Line 2: 2 NNNNN...
  std::istringstream stream(raw);
  std::vector<SatelliteTLE> result;
  std::string line;

  while (std::getline(stream, line)) {
    std::string name = trim(line);
    if (name.empty())
      continue;

    // Read line 1
    std::string l1;
    if (!std::getline(stream, l1))
      break;
    l1 = trim(l1);

    // Read line 2
    std::string l2;
    if (!std::getline(stream, l2))
      break;
    l2 = trim(l2);

    // Validate: line 1 starts with '1', line 2 starts with '2'
    if (l1.empty() || l2.empty())
      continue;
    if (l1[0] != '1' || l2[0] != '2')
      continue;

    SatelliteTLE tle;
    tle.name = name;
    tle.line1 = l1;
    tle.line2 = l2;

    // Extract NORAD ID from line 1 (columns 3-7)
    if (l1.size() >= 7) {
      tle.noradId = std::atoi(l1.substr(2, 5).c_str());
    }

    result.push_back(std::move(tle));
  }

  LOG_I("SatelliteManager", "Parsed {} satellites", result.size());

  std::lock_guard<std::mutex> lock(mutex_);
  satellites_ = std::move(result);
  dataValid_ = true;
  lastFetch_ = std::chrono::steady_clock::now();
}

std::vector<SatelliteTLE> SatelliteManager::getSatellites() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return satellites_;
}

bool SatelliteManager::hasData() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return dataValid_;
}

const SatelliteTLE *SatelliteManager::findByNoradId(int noradId) const {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &sat : satellites_) {
    if (sat.noradId == noradId)
      return &sat;
  }
  return nullptr;
}

const SatelliteTLE *
SatelliteManager::findByName(const std::string &search) const {
  std::lock_guard<std::mutex> lock(mutex_);

  // Case-insensitive substring search
  std::string lower;
  lower.resize(search.size());
  std::transform(search.begin(), search.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  for (const auto &sat : satellites_) {
    std::string satLower;
    satLower.resize(sat.name.size());
    std::transform(sat.name.begin(), sat.name.end(), satLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (satLower.find(lower) != std::string::npos)
      return &sat;
  }
  return nullptr;
}
