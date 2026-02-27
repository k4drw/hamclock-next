#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace HamClock {

struct ReachData {
    bool valid = false;
    uint32_t lastUpdate = 0;
    
    std::string band;
    std::string mode;
    int spotCount = 0;
    
    // 660x330 grid of density/heat values [0.0 - 1.0]
    std::vector<float> grid;
    
    static constexpr int GRID_W = 660;
    static constexpr int GRID_H = 330;
};

} // namespace HamClock
