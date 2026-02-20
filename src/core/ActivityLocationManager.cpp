#include "ActivityLocationManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include "WorkerService.h"
#include <sstream>
#include <algorithm>
#include <cstring>
#include <fstream>

bool POTAPark::operator<(const POTAPark& other) const {
    return std::strcmp(reference, other.reference) < 0;
}

bool SOTASummit::operator<(const SOTASummit& other) const {
    return std::strcmp(reference, other.reference) < 0;
}

ActivityLocationManager& ActivityLocationManager::getInstance() {
    static ActivityLocationManager instance;
    return instance;
}

void ActivityLocationManager::init(NetworkManager& net, const std::filesystem::path& cacheDir) {
    cacheDir_ = cacheDir;
    net_ = &net;
    loadApiCache();

    // Check for a pre-seeded summitslist.csv in configDir or cwd before hitting the network
    std::filesystem::path seedLocations[] = {
        cacheDir_.parent_path() / "summitslist.csv",
        std::filesystem::current_path() / "summitslist.csv"
    };
    for (const auto& p : seedLocations) {
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            LOG_I("ActivityLoc", "Found pre-seeded SOTA CSV at {}", p.string());
            std::ifstream ifs(p);
            if (ifs) {
                std::string data((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
                WorkerService::getInstance().submitTask([this, data = std::move(data)]() {
                    parseSOTA(data);
                });
            }
            break;
        }
    }

    fetchAndLoad(net);
}

bool ActivityLocationManager::getPOTALocation(const std::string& ref, float& lat, float& lon) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (potaParks_.empty()) return false;

    POTAPark target;
    std::strncpy(target.reference, ref.c_str(), sizeof(target.reference) - 1);
    
    auto it = std::lower_bound(potaParks_.begin(), potaParks_.end(), target);
    if (it != potaParks_.end() && std::strcmp(it->reference, target.reference) == 0) {
        lat = it->lat;
        lon = it->lon;
        return true;
    }
    return false;
}

bool ActivityLocationManager::getSOTALocation(const std::string& ref, float& lat, float& lon) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!sotaSummits_.empty()) {
        SOTASummit target;
        std::strncpy(target.reference, ref.c_str(), sizeof(target.reference) - 1);
        target.reference[sizeof(target.reference) - 1] = '\0';

        auto it = std::lower_bound(sotaSummits_.begin(), sotaSummits_.end(), target);
        if (it != sotaSummits_.end() && std::strcmp(it->reference, target.reference) == 0) {
            lat = it->lat;
            lon = it->lon;
            return true;
        }
    }

    // Fallback: per-summit API cache (populated by resolveSummitAsync)
    auto cit = sotaApiCache_.find(ref);
    if (cit != sotaApiCache_.end()) {
        lat = cit->second.first;
        lon = cit->second.second;
        return true;
    }

    return false;
}

void ActivityLocationManager::fetchAndLoad(NetworkManager& net) {
    // Fetch POTA
    net.fetchAsync(POTA_CSV_URL, [this](std::string data) {
        if (data.empty()) {
            LOG_E("ActivityLoc", "Failed to fetch POTA CSV");
            return;
        }
        WorkerService::getInstance().submitTask([this, data = std::move(data)]() {
            parsePOTA(data);
        });
    }, 86400 * 7); // Cache for 7 days

    // Fetch SOTA
    net.fetchAsync(SOTA_CSV_URL, [this](std::string data) {
        if (data.empty()) {
            LOG_E("ActivityLoc", "Failed to fetch SOTA CSV");
            return;
        }
        WorkerService::getInstance().submitTask([this, data = std::move(data)]() {
            parseSOTA(data);
        });
    }, 86400 * 7);
}

// Lightweight CSV helper: splits a line into fields, handling quotes
static std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool inQuotes = false;
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);
    return fields;
}

void ActivityLocationManager::parsePOTA(const std::string& data) {
    LOG_I("ActivityLoc", "Parsing POTA data...");
    std::vector<POTAPark> parks;
    std::stringstream ss(data);
    std::string line;
    
    // Header: "reference","name","active","entityId","locationDesc","latitude","longitude","grid"
    if (!std::getline(ss, line)) return;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto fields = splitCSVLine(line);
        if (fields.size() >= 7) {
            POTAPark p;
            std::strncpy(p.reference, fields[0].c_str(), sizeof(p.reference) - 1);
            p.reference[sizeof(p.reference) - 1] = '\0';
            p.lat = StringUtils::safe_stof(fields[5]);
            p.lon = StringUtils::safe_stof(fields[6]);
            parks.push_back(p);
        }
    }

    std::sort(parks.begin(), parks.end());
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        potaParks_ = std::move(parks);
    }
    LOG_I("ActivityLoc", "Loaded {} POTA parks", potaParks_.size());
    ready_ = true;
}

void ActivityLocationManager::parseSOTA(const std::string& data) {
    LOG_I("ActivityLoc", "Parsing SOTA data...");
    std::vector<SOTASummit> summits;
    std::stringstream ss(data);
    std::string line;

    // summitslist.csv has TWO header lines:
    //   Line 1: "SOTA Summits List (Date=...)"
    //   Line 2: SummitCode,AssociationName,RegionName,SummitName,AltM,AltFt,GridRef1,GridRef2,Longitude,Latitude,...
    // Columns: [0]=SummitCode [6]=GridRef1 [7]=GridRef2 [8]=Longitude [9]=Latitude
    if (!std::getline(ss, line)) return; // title line
    if (!std::getline(ss, line)) return; // column header line

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto fields = splitCSVLine(line);
        if (fields.size() >= 10) {
            SOTASummit s;
            std::strncpy(s.reference, fields[0].c_str(), sizeof(s.reference) - 1);
            s.reference[sizeof(s.reference) - 1] = '\0';
            s.lat = StringUtils::safe_stof(fields[9]);  // Latitude column
            s.lon = StringUtils::safe_stof(fields[8]);  // Longitude column
            summits.push_back(s);
        }
    }

    std::sort(summits.begin(), summits.end());

    {
        std::lock_guard<std::mutex> lock(mutex_);
        sotaSummits_ = std::move(summits);
    }
    LOG_I("ActivityLoc", "Loaded {} SOTA summits", sotaSummits_.size());
}

void ActivityLocationManager::resolveSummitAsync(const std::string& ref) {
    if (!net_) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sotaApiCache_.count(ref) || sotaApiInFlight_.count(ref)) return;
        sotaApiInFlight_.insert(ref);
    }

    std::string url = std::string(SOTA_SUMMIT_API) + ref;
    net_->fetchAsync(url, [this, ref](std::string data) {
        if (data.empty()) {
            std::lock_guard<std::mutex> lock(mutex_);
            sotaApiInFlight_.erase(ref);
            return;
        }

        // Lightweight JSON field extraction: "latitude": val, "longitude": val
        auto extractField = [&](const std::string& key) -> float {
            auto pos = data.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0.0f;
            pos = data.find(':', pos);
            if (pos == std::string::npos) return 0.0f;
            ++pos;
            while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t')) ++pos;
            return StringUtils::safe_stof(data.substr(pos, 20));
        };

        float lat = extractField("latitude");
        float lon = extractField("longitude");

        if (lat == 0.0f && lon == 0.0f) {
            std::lock_guard<std::mutex> lock(mutex_);
            sotaApiInFlight_.erase(ref);
            return;
        }

        LOG_D("ActivityLoc", "Resolved SOTA {} via API: {},{}", ref, lat, lon);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            sotaApiCache_[ref] = {lat, lon};
            sotaApiInFlight_.erase(ref);
        }

        // Persist cache asynchronously
        WorkerService::getInstance().submitTask([this]() { saveApiCache(); });
    }, 86400 * 30); // Cache API responses for 30 days
}

void ActivityLocationManager::loadApiCache() {
    std::filesystem::path cachePath = cacheDir_ / "sota_api_cache.csv";
    std::ifstream ifs(cachePath);
    if (!ifs) return;

    std::string line;
    int count = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;

        std::string ref = line.substr(0, tab1);
        float lat = StringUtils::safe_stof(line.substr(tab1 + 1, tab2 - tab1 - 1));
        float lon = StringUtils::safe_stof(line.substr(tab2 + 1));

        if (!ref.empty()) {
            sotaApiCache_[ref] = {lat, lon};
            ++count;
        }
    }
    if (count > 0)
        LOG_I("ActivityLoc", "Loaded {} SOTA API cache entries", count);
}

void ActivityLocationManager::saveApiCache() {
    std::filesystem::path cachePath = cacheDir_ / "sota_api_cache.csv";
    std::error_code ec;
    std::filesystem::create_directories(cacheDir_, ec);

    std::unordered_map<std::string, std::pair<float,float>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        snapshot = sotaApiCache_;
    }

    std::ofstream ofs(cachePath, std::ios::trunc);
    if (!ofs) return;
    for (const auto& [ref, coords] : snapshot) {
        ofs << ref << '\t' << coords.first << '\t' << coords.second << '\n';
    }
}
