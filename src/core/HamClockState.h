#pragma once

#include "Astronomy.h"

#include <string>

struct HamClockState {
    // DE (home) station — set from config
    LatLon deLocation = {0, 0};
    std::string deCallsign;
    std::string deGrid;

    // DX (target) station — set by map clicks
    LatLon dxLocation = {0, 0};
    std::string dxGrid;
    bool dxActive = false;
};
