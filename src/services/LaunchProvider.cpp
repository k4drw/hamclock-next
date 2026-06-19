#include "LaunchProvider.h"
#include "../core/Logger.h"
#include <nlohmann/json.hpp>
#include <SDL.h>

using json = nlohmann::json;

LaunchProvider::LaunchProvider(NetworkManager& net) : net_(net), lastFetch_(0) {}

void LaunchProvider::fetch(bool force) {
    uint32_t now = SDL_GetTicks();
    // Fetch every 1 hour (3600000 ms)
    if (!force && lastFetch_ != 0 && (now - lastFetch_ < 3600000)) {
        return;
    }
    lastFetch_ = now;

    LOG_I("LaunchProvider", "Fetching upcoming launches...");

    std::weak_ptr<LaunchProvider> self = shared_from_this();
    net_.fetchAsync(API_URL, [self](std::string raw) {
        auto p = self.lock();
        if (!p) return;
        if (!raw.empty()) {
            p->parse(raw);
        } else {
            LOG_E("LaunchProvider", "Failed to fetch launches: empty response");
        }
    }, 3600, false);
}

void LaunchProvider::parse(const std::string& raw) {
    try {
        auto j = json::parse(raw);
        if (!j.contains("results") || !j["results"].is_array()) {
            LOG_E("LaunchProvider", "Invalid JSON structure from Launch Library");
            return;
        }

        std::vector<LaunchEvent> upcoming;
        for (const auto& item : j["results"]) {
            LaunchEvent ev;
            ev.id = item.value("id", "");
            ev.missionName = item.value("name", "Unknown Mission");
            
            if (item.contains("status") && item["status"].is_object()) {
                ev.statusName = item["status"].value("name", "Unknown");
            }
            
            if (item.contains("rocket") && item["rocket"].is_object() && 
                item["rocket"].contains("configuration") && item["rocket"]["configuration"].is_object()) {
                ev.rocketName = item["rocket"]["configuration"].value("name", "Unknown Rocket");
            }
            
            if (item.contains("launch_service_provider") && item["launch_service_provider"].is_object()) {
                ev.providerName = item["launch_service_provider"].value("name", "Unknown Provider");
            }

            ev.windowStart = 0;
            std::string netStr = item.value("net", "");
            if (!netStr.empty()) {
                struct tm tm {};
                // Format: 2026-06-15T18:00:00Z
                if (sscanf(netStr.c_str(), "%d-%d-%dT%d:%d:%dZ", 
                           &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
                           &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
                    tm.tm_year -= 1900;
                    tm.tm_mon -= 1;
#ifdef _WIN32
                    ev.windowStart = _mkgmtime(&tm);
#else
                    ev.windowStart = timegm(&tm);
#endif
                }
            }

            ev.hasLocation = false;
            if (item.contains("pad") && item["pad"].is_object()) {
                auto pad = item["pad"];
                ev.padName = pad.value("name", "Unknown Pad");
                
                std::string latStr = pad.value("latitude", "");
                std::string lonStr = pad.value("longitude", "");
                
                if (!latStr.empty() && !lonStr.empty()) {
                    try {
                        ev.padLat = std::stod(latStr);
                        ev.padLon = std::stod(lonStr);
                        ev.hasLocation = true;
                    } catch (...) {
                        ev.hasLocation = false;
                    }
                }
            }

            upcoming.push_back(ev);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            upcomingLaunches_ = std::move(upcoming);
            dataValid_ = true;
        }

        LOG_I("LaunchProvider", "Successfully parsed {} upcoming launches.", upcomingLaunches_.size());

    } catch (const std::exception& e) {
        LOG_E("LaunchProvider", "Failed to parse launches: {}", e.what());
    }
}

std::vector<LaunchEvent> LaunchProvider::getUpcoming() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return upcomingLaunches_;
}
