#include "SatelliteManager.h"
#include "../services/RotatorService.h"
#include "Logger.h"

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
  // Deprecated: Tracking logic moved to RotatorService::pollLoop
  // This function is kept for API compatibility but is now a no-op
}

void SatelliteManager::trackSatellite(const std::string &satName) {
  std::lock_guard<std::mutex> lock(mutex_);
  trackedSatName_ = satName;

  if (rotator_) {
    if (satName.empty()) {
      currentSat_.reset();
      rotator_->stopAutoTrack();
    } else {
      // Find the TLE for this satellite
      auto it = std::find_if(
          satellites_.begin(), satellites_.end(), [&](const SatelliteTLE &s) {
            // Case-insensitive compare
            std::string s1 = s.name;
            std::string s2 = satName;
            std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
            std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
            return s1 == s2;
          });

      if (it != satellites_.end()) {
        currentSat_ = std::make_unique<Satellite>(*it);
        currentSat_->setObserver(obsLat_, obsLon_);
        rotator_->autoTrack(currentSat_.get());
      } else {
        LOG_W("SatManager", "Cannot track '{}': TLE not found", satName);
        currentSat_.reset();
        rotator_->stopAutoTrack();
      }
    }
  }
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

  // If we are currently tracking a satellite, we should update the TLE
  // in RotatorService in case it changed.
  if (!trackedSatName_.empty() && rotator_) {
    // We need to re-find the satellite in the new list
    // Note: we are holding the lock here, so we can't call trackSatellite()
    // directly if it also takes the lock. But trackSatellite takes the lock.
    // So we should find it here and call rotator directly.

    auto it = std::find_if(
        satellites_.begin(), satellites_.end(), [&](const SatelliteTLE &s) {
          std::string s1 = s.name;
          std::string s2 = trackedSatName_;
          std::transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
          std::transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
          return s1 == s2;
        });

    if (it != satellites_.end()) {
      currentSat_ = std::make_unique<Satellite>(*it);
      currentSat_->setObserver(obsLat_, obsLon_);
      rotator_->autoTrack(currentSat_.get()); // Update TLE
    }
  }
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
