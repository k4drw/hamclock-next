#pragma once

#include "../network/NetworkManager.h"

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct SatelliteTLE {
    std::string name;   // Satellite name (trimmed)
    std::string line1;  // TLE line 1
    std::string line2;  // TLE line 2
    int noradId = 0;    // NORAD catalog number (from line 1)
};

class SatelliteManager {
public:
    explicit SatelliteManager(NetworkManager& net);

    // Trigger a TLE fetch (async). Safe to call repeatedly; will skip if
    // data is fresh (< 24h old) unless force=true.
    void fetch(bool force = false);

    // Thread-safe snapshot of the current TLE list.
    std::vector<SatelliteTLE> getSatellites() const;

    // True once at least one successful fetch has completed.
    bool hasData() const;

    // Find a satellite by NORAD ID. Returns nullptr if not found.
    // The returned pointer is invalidated by the next fetch.
    const SatelliteTLE* findByNoradId(int noradId) const;

    // Find a satellite by (partial, case-insensitive) name match.
    const SatelliteTLE* findByName(const std::string& search) const;

private:
    void parse(const std::string& raw);

    static constexpr const char* TLE_URL =
        "https://celestrak.org/NORAD/elements/gp.php?GROUP=amateur&FORMAT=tle";

    NetworkManager& net_;

    mutable std::mutex mutex_;
    std::vector<SatelliteTLE> satellites_;
    bool dataValid_ = false;
    std::chrono::steady_clock::time_point lastFetch_;
};
