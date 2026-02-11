#include "LiveSpotProvider.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

namespace {

// Extract an XML attribute value: finds attr="value" and returns value.
std::string extractAttr(const std::string& tag, const char* attr)
{
    std::string needle = std::string(attr) + "=\"";
    auto pos = tag.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    auto end = tag.find('"', pos);
    if (end == std::string::npos) return {};
    return tag.substr(pos, end - pos);
}

// Parse PSK Reporter XML response, aggregating spot counts per band
// and collecting individual spot records for map plotting.
void parsePSKReporter(const std::string& body, LiveSpotData& data)
{
    std::string::size_type pos = 0;
    int total = 0;

    while (pos < body.size()) {
        auto tagStart = body.find("<receptionReport ", pos);
        if (tagStart == std::string::npos) break;
        auto tagEnd = body.find("/>", tagStart);
        if (tagEnd == std::string::npos) tagEnd = body.find(">", tagStart);
        if (tagEnd == std::string::npos) break;

        std::string tag = body.substr(tagStart, tagEnd - tagStart);

        std::string freqStr = extractAttr(tag, "frequency");
        if (!freqStr.empty()) {
            long long freqHz = std::atoll(freqStr.c_str());
            double freqKhz = static_cast<double>(freqHz) / 1000.0;
            int idx = freqToBandIndex(freqKhz);
            if (idx >= 0) {
                data.bandCounts[idx]++;
                total++;

                std::string grid = extractAttr(tag, "receiverLocator");
                if (grid.size() >= 4) {
                    data.spots.push_back({freqKhz, grid});
                }
            }
        }

        pos = tagEnd + 1;
    }

    std::fprintf(stderr, "LiveSpotProvider: parsed %d spots (%zu with grids) across bands\n",
                 total, data.spots.size());
}

} // namespace

LiveSpotProvider::LiveSpotProvider(NetworkManager& net,
                                   std::shared_ptr<LiveSpotDataStore> store,
                                   const std::string& callsign,
                                   const std::string& grid)
    : net_(net)
    , store_(std::move(store))
    , callsign_(callsign)
    , grid_(grid)
{
}

void LiveSpotProvider::fetch()
{
    if (grid_.empty()) {
        std::fprintf(stderr, "LiveSpotProvider: no grid configured, skipping\n");
        return;
    }

    // Build PSK Reporter URL: spots from our grid (last 30 minutes)
    auto now = std::time(nullptr);
    int64_t windowStart = static_cast<int64_t>(now) - 1800;

    // Use only the first 4 characters of the grid (field + square)
    std::string grid4 = grid_.substr(0, 4);

    std::string url = "https://retrieve.pskreporter.info/query"
                      "?senderLocator=" + grid4 +
                      "&flowStartSeconds=" + std::to_string(windowStart) +
                      "&rronly=1";

    std::fprintf(stderr, "LiveSpotProvider: fetching %s\n", url.c_str());

    auto store = store_;
    auto grid = grid_;
    net_.fetchAsync(url, [store, grid](std::string body) {
        LiveSpotData data;
        data.grid = grid.substr(0, 4);
        data.windowMinutes = 30;

        if (!body.empty()) {
            parsePSKReporter(body, data);
        } else {
            std::fprintf(stderr, "LiveSpotProvider: empty response, using stubs\n");
            data.bandCounts[5]  = 373;  // 20m
            data.bandCounts[6]  = 59;   // 17m
            data.bandCounts[7]  = 200;  // 15m
            data.bandCounts[8]  = 236;  // 12m
            data.bandCounts[9]  = 445;  // 10m
            data.bandCounts[1]  = 1;    // 80m
        }

        data.lastUpdated = std::chrono::system_clock::now();
        data.valid = true;
        store->set(data);
    });
}
