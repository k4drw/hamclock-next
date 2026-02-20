#pragma once

#include "../network/NetworkManager.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

struct POTAPark {
    char reference[16];
    float lat;
    float lon;

    bool operator<(const POTAPark& other) const;
};

struct SOTASummit {
    char reference[16];
    float lat;
    float lon;

    bool operator<(const SOTASummit& other) const;
};

class ActivityLocationManager {
public:
    static ActivityLocationManager& getInstance();

    // Initialization: kicks off background fetch if needed
    void init(NetworkManager& net, const std::filesystem::path& cacheDir);

    // Coordinate lookups (thread-safe)
    bool getPOTALocation(const std::string& ref, float& lat, float& lon);
    bool getSOTALocation(const std::string& ref, float& lat, float& lon);

    // Async per-summit API lookup; updates sotaApiCache_ when resolved
    void resolveSummitAsync(const std::string& ref);

    bool isReady() const { return ready_.load(); }

private:
    ActivityLocationManager() = default;
    ~ActivityLocationManager() = default;
    ActivityLocationManager(const ActivityLocationManager&) = delete;
    ActivityLocationManager& operator=(const ActivityLocationManager&) = delete;

    void fetchAndLoad(NetworkManager& net);
    void parsePOTA(const std::string& csvData);
    void parseSOTA(const std::string& csvData);
    void loadApiCache();
    void saveApiCache();

    std::vector<POTAPark> potaParks_;
    std::vector<SOTASummit> sotaSummits_;

    // Per-summit API cache (fallback for summits not in bulk CSV)
    std::unordered_map<std::string, std::pair<float,float>> sotaApiCache_;
    std::unordered_set<std::string> sotaApiInFlight_;

    NetworkManager* net_ = nullptr;
    mutable std::mutex mutex_;
    std::atomic<bool> ready_{false};
    std::filesystem::path cacheDir_;

    static constexpr const char* POTA_CSV_URL = "https://pota.app/all_parks_ext.csv";
    static constexpr const char* SOTA_CSV_URL = "https://storage.sota.org.uk/summitslist.csv";
    static constexpr const char* SOTA_SUMMIT_API = "https://api2.sota.org.uk/api/summits/";
};
