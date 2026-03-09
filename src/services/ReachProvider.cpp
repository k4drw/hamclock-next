#include "ReachProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/Astronomy.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

namespace HamClock {

ReachProvider::ReachProvider(NetworkManager &net, std::shared_ptr<HamClockState> state)
    : net_(net), state_(state) {
    data_.grid.assign(ReachData::GRID_W * ReachData::GRID_H, 0.0f);
}

void ReachProvider::fetch(const std::string& band, const std::string& mode) {
    fetchPSK(band, mode);
}

void ReachProvider::fetchPSK(const std::string& band, const std::string& mode) {
    // PSK Reporter API: Query for signals heard BY our callsign (or heard US if configured)
    // For "Reach Heatmap", we usually want to see who heard US (transmitter reach).
    std::string call;
    {
        std::lock_guard<std::mutex> lk(state_->locationMutex);
        call = state_->deCallsign;
    }
    if (call.empty()) call = "NOCALL";

    // Example URL: https://pskreporter.info/query?senderCallsign=K1ABC&flowctrl=1
    char url[256];
    std::snprintf(url, sizeof(url), 
        "https://pskreporter.info/query?senderCallsign=%s&flowctrl=1", 
        call.c_str());

    LOG_I("ReachProvider", "Fetching PSK reach for {}: {}", call, url);

    std::weak_ptr<ReachProvider> self = shared_from_this();
    net_.fetchAsync(url, [self, band, mode](std::string body) {
        auto p = self.lock();
        if (p) {
            if (!body.empty()) {
                p->processPSK(body, band, mode);
            } else {
                LOG_E("ReachProvider", "PSK fetch failed or empty");
            }
        }
    }, 300); // Cache for 5 mins
}

void ReachProvider::processPSK(const std::string& body, const std::string& band, const std::string& mode) {
    std::vector<std::pair<double, double>> spots;
    
    std::string::size_type pos = 0;
    while (pos < body.size()) {
        auto tagStart = body.find("<receptionReport ", pos);
        if (tagStart == std::string::npos) break;
        auto tagEnd = body.find("/>", tagStart);
        if (tagEnd == std::string::npos) tagEnd = body.find(">", tagStart);
        if (tagEnd == std::string::npos) break;

        std::string tag = body.substr(tagStart, tagEnd - tagStart);
        
        // Filter by band/mode if specified
        // (Note: Simplified for initial implementation)
        
        std::string grid = StringUtils::extractAttr(tag, "receiverLocator");
        if (grid.size() >= 4) {
            double lat, lon;
            if (Astronomy::gridToLatLon(grid, lat, lon)) {
                spots.push_back({lat, lon});
            }
        }
        pos = tagEnd + 1;
    }

    LOG_I("ReachProvider", "Processing {} reach spots", spots.size());
    applyHeatmap(spots, band, mode);
}

void ReachProvider::applyHeatmap(const std::vector<std::pair<double, double>>& spots, 
                               const std::string& band, const std::string& mode) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    data_.grid.assign(ReachData::GRID_W * ReachData::GRID_H, 0.0f);
    data_.band = band;
    data_.mode = mode;
    data_.spotCount = (int)spots.size();
    data_.lastUpdate = SDL_GetTicks();
    data_.valid = true;

    if (spots.empty()) {
        if (callback_) callback_(data_);
        return;
    }

    // Accumulate spot counts into grid
    for (const auto& spot : spots) {
        // Map lat/lon to grid [660x330]
        // Lon: -180..180 -> 0..660
        // Lat: 90..-90 -> 0..330
        int gx = static_cast<int>((spot.second + 180.0) * ReachData::GRID_W / 360.0);
        int gy = static_cast<int>((90.0 - spot.first) * ReachData::GRID_H / 180.0);
        
        gx = std::clamp(gx, 0, ReachData::GRID_W - 1);
        gy = std::clamp(gy, 0, ReachData::GRID_H - 1);
        
        data_.grid[gy * ReachData::GRID_W + gx] += 1.0f;
    }

    // Simple Gaussian Blur (Box Blur pass) for "heat" effect
    std::vector<float> blurred = data_.grid;
    const int radius = 3;
    for (int j = radius; j < ReachData::GRID_H - radius; ++j) {
        for (int i = radius; i < ReachData::GRID_W - radius; ++i) {
            float sum = 0;
            for (int ky = -radius; ky <= radius; ++ky) {
                for (int kx = -radius; kx <= radius; ++kx) {
                    sum += data_.grid[(j + ky) * ReachData::GRID_W + (i + kx)];
                }
            }
            blurred[j * ReachData::GRID_W + i] = sum / ((2 * radius + 1) * (2 * radius + 1));
        }
    }

    // Normalize
    float maxVal = 0.01f;
    for (float v : blurred) if (v > maxVal) maxVal = v;
    for (float &v : blurred) v /= maxVal;

    data_.grid = std::move(blurred);

    if (callback_) callback_(data_);
}

ReachData ReachProvider::getData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

} // namespace HamClock
