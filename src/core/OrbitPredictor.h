#pragma once

#include "SatelliteTypes.h"

#include <predict/predict.h>

#include <ctime>
#include <string>
#include <vector>

// RAII wrapper around libpredict's C API.
class OrbitPredictor {
public:
  OrbitPredictor();
  ~OrbitPredictor();

  OrbitPredictor(const OrbitPredictor &) = delete;
  OrbitPredictor &operator=(const OrbitPredictor &) = delete;

  // Set the observer location (call once or when QTH changes).
  void setObserver(double latDeg, double lonDeg, double altMeters = 0.0);

  // Load a satellite from TLE data. Returns false if TLE is invalid.
  bool loadTLE(const SatelliteTLE &tle);

  // True if both observer and TLE are loaded.
  bool isReady() const;

  // Get the satellite name.
  const std::string &satName() const { return satName_; }

  // --- Real-time queries (use current system time) ---

  // Observe satellite from observer position at current time.
  SatObservation observe() const;

  // Get sub-satellite point at current time.
  SubSatPoint subSatPoint() const;

  // --- Time-parameterized queries ---

  // Observe at a specific UTC time.
  SatObservation observeAt(std::time_t utc) const;

  // Sub-satellite point at a specific UTC time.
  SubSatPoint subSatPointAt(std::time_t utc) const;

  // Find the next pass from the current time.
  SatPass nextPass() const;

  // Find the next pass from a given time.
  SatPass nextPassAfter(std::time_t utc) const;

  // Calculate ground track for the next `minutes` from a given time.
  // Returns points at `stepSec` intervals.
  std::vector<GroundTrackPoint>
  groundTrack(std::time_t startUtc, int minutes = 90, int stepSec = 30) const;

  // Calculate Doppler shift for a given downlink frequency (Hz).
  // Returns frequency offset in Hz.
  double dopplerShift(double downlinkHz) const;

  // TLE age in days (current time minus TLE epoch). Returns -1 if not loaded.
  double tleAgeDays() const;

private:
  static constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
  static constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;

  predict_observer_t *observer_ = nullptr;
  predict_orbital_elements_t *elements_ = nullptr;
  std::string satName_;
};
