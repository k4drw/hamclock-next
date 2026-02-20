#pragma once

#include "IonosondeData.h"
#include "SolarData.h"
#include <string>
#include <vector>

struct PropPathParams {
  double txLat;
  double txLon;
  double mhz;
  double watts;
  std::string mode; // "SSB", "CW", "FT8", etc.
  int toa;          // Take-off angle (approx)
  int path;         // 0=Short, 1=Long
};

class PropEngine {
public:
  static constexpr int MAP_W = 660;
  static constexpr int MAP_H = 330;

  /**
   * Calculate signal margin in dB based on mode and power.
   * Baseline is 100W SSB = 0 dB.
   */
  static double calculateSignalMargin(const std::string &mode, double watts);

  /**
   * Calculate Maximum Usable Frequency (MUF) for a path.
   * @param distKm Path distance
   * @param midLat Midpoint latitude
   * @param midLon Midpoint longitude
   * @param hour UTC hour
   * @param sfi Solar Flux Index
   * @param ssn Sunspot Number
   * @param ionoData Interpolated ionosonde data (optional)
   */
  static double calculateMUF(double distKm, double midLat, double midLon,
                             double hour, double sfi, double ssn,
                             const InterpolatedIonosonde &ionoData);

  /**
   * Calculate Lowest Usable Frequency (LUF).
   */
  static double calculateLUF(double distKm, double midLat, double hour,
                             double sfi, double kIndex);

  /**
   * Calculate reliability (0.0 to 1.0) for a given frequency.
   */
  static double calculateReliability(double freqMhz, double distKm,
                                     double midLat, double midLon, double hour,
                                     double sfi, double ssn, double kIndex,
                                     const InterpolatedIonosonde &ionoData,
                                     double currentHour, double signalMarginDb);

  /**
   * Calculate take-off angle (degrees) for a path from TX to a grid point.
   * Returns 0 if no propagation is possible (freq > MUF).
   * @param distKm Path distance
   * @param muf Maximum Usable Frequency for this path
   * @param freqMhz Operating frequency
   * @return Elevation angle in degrees (0 = no path, ~1-40 = valid path)
   */
  static double calculateTOA(double distKm, double muf, double freqMhz);

  /**
   * Generate a 660x330 grid of values.
   * @param params Transmission parameters
   * @param swSpaceWeather Current space weather (SFI, SSN, K, etc.)
   * @param ionoProvider Reference to provider for fetching iono data per-point
   * @param outputType 0=MUF, 1=Reliability, 2=TOA (take-off angle)
   * @return vector of floats (0-100 for Rel, 0-50 for MUF, 0-40 for TOA degrees)
   */
  static std::vector<float>
  generateGrid(const PropPathParams &params, const SolarData &sw,
               const class IonosondeProvider *ionoProvider, int outputType);
};
