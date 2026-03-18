#include "ReachProvider.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include "../core/Astronomy.h"
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <unordered_set>

namespace HamClock {

// ── Helpers (local) ───────────────────────────────────────────────────────────

namespace {

// URL-encode a SQL query string for ClickHouse HTTP GET.
std::string urlEncode(const std::string &s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else if (c == ' ') {
            out += '+';
        } else {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

// Parse one field from a ClickHouse FORMAT CSV line (handles quoted fields).
std::string csvField(const std::string &line, size_t &pos) {
    if (pos >= line.size()) return {};
    std::string result;
    if (line[pos] == '"') {
        ++pos;
        while (pos < line.size()) {
            if (line[pos] == '"') {
                ++pos;
                if (pos < line.size() && line[pos] == '"') {
                    result += '"';
                    ++pos;
                } else {
                    break;
                }
            } else {
                result += line[pos++];
            }
        }
    } else {
        while (pos < line.size() && line[pos] != ',')
            result += line[pos++];
    }
    if (pos < line.size() && line[pos] == ',')
        ++pos;
    return result;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ReachProvider::ReachProvider(NetworkManager &net,
                             std::shared_ptr<HamClockState> state,
                             std::shared_ptr<LiveSpotDataStore> spotStore)
    : net_(net), state_(state), spotStore_(std::move(spotStore)) {
    data_.grid.assign(ReachData::GRID_W * ReachData::GRID_H, 0.0f);
}

void ReachProvider::fetch(const std::string& band, const std::string& mode) {
    lastFetchMs_ = SDL_GetTicks();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingSpots_.clear();
    }
    pendingFetches_.store(2);
    fetchPSK(band, mode);
    fetchWSPR(band, mode);
}

// ── PSK Reporter ─────────────────────────────────────────────────────────────

void ReachProvider::fetchPSK(const std::string& band, const std::string& mode) {
    std::string grid = state_->deGrid;
    if (grid.size() < 4) {
        LOG_E("ReachProvider", "DE grid not set, skipping PSK fetch");
        onSourceComplete(band, mode);
        return;
    }
    std::string grid4 = grid.substr(0, 4);

    char url[256];
    std::snprintf(url, sizeof(url),
        "https://pskreporter.info/query?senderLocator=%s&flowctrl=1",
        grid4.c_str());

    LOG_I("ReachProvider", "Fetching PSK reach for grid {}: {}", grid4, url);

    std::weak_ptr<ReachProvider> self = shared_from_this();
    net_.fetchAsync(url, [self, band, mode](std::string body) {
        auto p = self.lock();
        if (!p) return;
        if (!body.empty())
            p->processPSK(body, band, mode);
        else
            LOG_E("ReachProvider", "PSK fetch failed or empty");
        p->onSourceComplete(band, mode);
    }, 300);
}

void ReachProvider::processPSK(const std::string& body, const std::string& band, const std::string& mode) {
    std::vector<SpotEntry> spots;

    std::string::size_type pos = 0;
    while (pos < body.size()) {
        auto tagStart = body.find("<receptionReport ", pos);
        if (tagStart == std::string::npos) break;
        auto tagEnd = body.find("/>", tagStart);
        if (tagEnd == std::string::npos) tagEnd = body.find(">", tagStart);
        if (tagEnd == std::string::npos) break;

        std::string tag = body.substr(tagStart, tagEnd - tagStart);

        std::string grid = StringUtils::extractAttr(tag, "receiverLocator");
        std::string call = StringUtils::extractAttr(tag, "receiverCallsign");
        if (grid.size() >= 4) {
            double lat, lon;
            if (Astronomy::gridToLatLon(grid, lat, lon))
                spots.push_back({lat, lon, call});
        }
        pos = tagEnd + 1;
    }

    LOG_I("ReachProvider", "PSK: {} spots parsed", spots.size());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingSpots_.insert(pendingSpots_.end(), spots.begin(), spots.end());
    }
}

// ── WSPR (db1.wspr.live) ──────────────────────────────────────────────────────

void ReachProvider::fetchWSPR(const std::string& band, const std::string& mode) {
    // If we have a shared LiveSpotDataStore with fresh data, drain it directly
    // to avoid a redundant API call.
    if (spotStore_) {
        auto snap = spotStore_->snapshot();
        if (snap->valid && !snap->spots.empty()) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - snap->lastUpdated).count();
            if (age < 300) { // < 5 minutes
                std::vector<SpotEntry> spots;
                for (const auto& s : snap->spots) {
                    double lat, lon;
                    if (s.receiverGrid.size() >= 4 &&
                        Astronomy::gridToLatLon(s.receiverGrid, lat, lon)) {
                        spots.push_back({lat, lon, s.senderCallsign});
                    }
                }
                LOG_I("ReachProvider", "WSPR: reused {} spots from LiveSpotDataStore (age {}s)",
                      spots.size(), age);
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    pendingSpots_.insert(pendingSpots_.end(), spots.begin(), spots.end());
                }
                onSourceComplete(band, mode);
                return;
            }
        }
    }

    // Fresh fetch from db1.wspr.live via ClickHouse HTTP.
    std::string grid = state_->deGrid;
    if (grid.size() < 4) {
        LOG_E("ReachProvider", "DE grid not set, skipping WSPR fetch");
        onSourceComplete(band, mode);
        return;
    }
    std::string grid4 = grid.substr(0, 4);

    // I am the transmitter (tx); show who heard me (rx).
    // Columns: time, tx_loc, tx_sign, rx_loc, rx_sign, 'WSPR', freq_hz, snr
    std::string sql =
        "SELECT toUnixTimestamp(time),tx_loc,tx_sign,rx_loc,rx_sign,"
        "'WSPR',cast(frequency as UInt64),snr "
        "FROM wspr.rx "
        "WHERE time > now()-3600 AND tx_loc LIKE '" + grid4 + "%' "
        "ORDER BY time DESC LIMIT 500 FORMAT CSV";

    std::string url = "http://db1.wspr.live/?query=" + urlEncode(sql);
    LOG_I("ReachProvider", "Fetching WSPR reach for grid {} via db1.wspr.live", grid4);

    std::weak_ptr<ReachProvider> self = shared_from_this();
    net_.fetchAsync(url, [self, band, mode](std::string body) {
        auto p = self.lock();
        if (!p) return;
        if (!body.empty())
            p->processWSPR(body, band, mode);
        else
            LOG_E("ReachProvider", "WSPR fetch failed or empty");
        p->onSourceComplete(band, mode);
    }, 300);
}

void ReachProvider::processWSPR(const std::string& body, const std::string& band, const std::string& mode) {
    std::vector<SpotEntry> spots;

    std::istringstream ss(body);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t pos = 0;
        csvField(line, pos);              // col 0: time
        csvField(line, pos);              // col 1: tx_loc (us)
        csvField(line, pos);              // col 2: tx_sign (us)
        std::string rxLoc  = csvField(line, pos); // col 3: rx_loc
        std::string rxSign = csvField(line, pos); // col 4: rx_sign

        if (rxLoc.size() >= 4) {
            double lat, lon;
            if (Astronomy::gridToLatLon(rxLoc, lat, lon))
                spots.push_back({lat, lon, rxSign});
        }
    }

    LOG_I("ReachProvider", "WSPR: {} spots parsed from db1.wspr.live", spots.size());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingSpots_.insert(pendingSpots_.end(), spots.begin(), spots.end());
    }
}

// ── Merge & apply ─────────────────────────────────────────────────────────────

void ReachProvider::onSourceComplete(const std::string& band, const std::string& mode) {
    if (--pendingFetches_ == 0)
        applyHeatmap(band, mode);
}

void ReachProvider::applyHeatmap(const std::string& band, const std::string& mode) {
    std::lock_guard<std::mutex> lock(mutex_);

    data_.grid.assign(ReachData::GRID_W * ReachData::GRID_H, 0.0f);
    data_.band = band;
    data_.mode = mode;
    data_.lastUpdate = SDL_GetTicks();
    data_.valid = true;

    if (pendingSpots_.empty()) {
        data_.spotCount = 0;
        if (callback_) callback_(data_);
        return;
    }

    // Deduplicate by callsign — one grid vote per unique receiving station
    std::unordered_set<std::string> seen;
    std::vector<std::pair<double, double>> unique;
    for (const auto& s : pendingSpots_) {
        if (!s.callsign.empty()) {
            if (!seen.insert(s.callsign).second)
                continue;
        }
        unique.push_back({s.lat, s.lon});
    }

    data_.spotCount = static_cast<int>(unique.size());
    LOG_I("ReachProvider", "Heatmap: {} unique stations (PSK+WSPR)", data_.spotCount);

    for (const auto& spot : unique) {
        int gx = static_cast<int>((spot.second + 180.0) * ReachData::GRID_W / 360.0);
        int gy = static_cast<int>((90.0 - spot.first) * ReachData::GRID_H / 180.0);
        gx = std::clamp(gx, 0, ReachData::GRID_W - 1);
        gy = std::clamp(gy, 0, ReachData::GRID_H - 1);
        data_.grid[gy * ReachData::GRID_W + gx] += 1.0f;
    }

    // Box blur (radius 3)
    std::vector<float> blurred = data_.grid;
    const int radius = 3;
    for (int j = radius; j < ReachData::GRID_H - radius; ++j) {
        for (int i = radius; i < ReachData::GRID_W - radius; ++i) {
            float sum = 0;
            for (int ky = -radius; ky <= radius; ++ky)
                for (int kx = -radius; kx <= radius; ++kx)
                    sum += data_.grid[(j + ky) * ReachData::GRID_W + (i + kx)];
            blurred[j * ReachData::GRID_W + i] = sum / ((2 * radius + 1) * (2 * radius + 1));
        }
    }

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
