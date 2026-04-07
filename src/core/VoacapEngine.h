#pragma once

#include "PropEngine.h"   // for PropPathParams (shared struct)
#include "SolarData.h"
#include <vector>

/**
 * VoacapEngine — CCIR/VOACAP HF propagation engine.
 *
 * Drop-in replacement for PropEngine for VOACAP-based overlays.
 * Uses CCIR ionospheric coefficient tables (embedded at build time by
 * gen_ccir_coeffs.py) to compute foF2, M(3000)F2, and foE via the
 * same VIRTIM + VERSY algorithm used in the original VOACAP Fortran code.
 *
 * Handles outputType:
 *   0 = F2 MUF (MHz, 0-50 range)
 *   1 = Circuit reliability (%, 0-100 range)
 *   2 = Take-off angle (degrees, 0-40 range)
 *
 * The real-time MUF overlay (PropOverlayType::Muf, with ionosonde data)
 * continues to use PropEngine.
 */
class VoacapEngine {
public:
    static constexpr int MAP_W = 660;
    static constexpr int MAP_H = 330;

    static std::vector<float> generateGrid(const PropPathParams &params,
                                           const SolarData &sw,
                                           const class IonosondeProvider *iono,
                                           int outputType);

    /**
     * Single-path circuit reliability using the CCIR/VOACAP model.
     *
     * @param freqMHz   Operating frequency (MHz)
     * @param distKm    Great-circle path distance (km)
     * @param midLatDeg Path midpoint latitude (degrees)
     * @param midLonDeg Path midpoint longitude (degrees)
     * @param utcHour   UTC hour (0-23)
     * @param month     Month (1-12)
     * @param ssn       Sunspot number (clamped to 0-100 internally)
     * @param watts     Transmitter power (watts)
     * @param mode      Mode string ("SSB", "CW", "FT8", etc.)
     * @param minToaDeg Minimum take-off angle filter (degrees); 0 = no filter
     * @return Circuit reliability (0-100 %), or 0 if path not supported
     */
    static double pathReliability(double freqMHz, double distKm,
                                  double midLatDeg, double midLonDeg,
                                  int utcHour, int month,
                                  double ssn, double watts,
                                  const std::string &mode,
                                  double minToaDeg = 3.0);
};
