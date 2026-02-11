#include "NOAAProvider.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>

NOAAProvider::NOAAProvider(NetworkManager& net, std::shared_ptr<SolarDataStore> store)
    : net_(net)
    , store_(std::move(store))
{
}

void NOAAProvider::fetch()
{
    // Capture store by value (shared_ptr copy) so it stays alive in the detached thread
    auto store = store_;

    net_.fetchAsync(K_INDEX_URL, [store](std::string body) {
        if (body.empty()) {
            std::fprintf(stderr, "NOAAProvider: empty response, skipping update\n");
            return;
        }

        auto j = nlohmann::json::parse(body, nullptr, false);
        if (j.is_discarded() || !j.is_array() || j.size() < 2) {
            std::fprintf(stderr, "NOAAProvider: invalid JSON response\n");
            return;
        }

        // Format: array of arrays. Row 0 = headers, last row = most recent.
        // Headers: ["time_tag", "Kp", "a_running", "station_count"]
        const auto& row = j.back();
        if (!row.is_array() || row.size() < 3) {
            std::fprintf(stderr, "NOAAProvider: unexpected row format\n");
            return;
        }

        SolarData data;
        try {
            // Kp is at index 1 (string like "2.33"), a_running at index 2
            double kp = std::stod(row[1].get<std::string>());
            data.k_index = static_cast<int>(kp);
            data.a_index = std::stoi(row[2].get<std::string>());
        } catch (const std::exception& e) {
            std::fprintf(stderr, "NOAAProvider: parse error: %s\n", e.what());
            return;
        }

        data.last_updated = std::chrono::system_clock::now();
        data.valid = true;
        store->set(data);

        std::fprintf(stderr, "NOAAProvider: K-index=%d, A-index=%d\n",
                     data.k_index, data.a_index);
    });
}
