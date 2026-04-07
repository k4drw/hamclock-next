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
};
