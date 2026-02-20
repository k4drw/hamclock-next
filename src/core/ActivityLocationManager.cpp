#include "ActivityLocationManager.h"
#include "Logger.h"
#include "StringUtils.h"
#include "WorkerService.h"
#include <sstream>
#include <algorithm>
#include <cstring>

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
    if (sotaSummits_.empty()) return false;

    SOTASummit target;
    std::strncpy(target.reference, ref.c_str(), sizeof(target.reference) - 1);

    auto it = std::lower_bound(sotaSummits_.begin(), sotaSummits_.end(), target);
    if (it != sotaSummits_.end() && std::strcmp(it->reference, target.reference) == 0) {
        lat = it->lat;
        lon = it->lon;
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

    // Header: SummitCode, AssociationName, RegionName, SummitName, AltM, AltFt, GridRef, Latitude, Longitude, Points, BonusPoints, ...
    if (!std::getline(ss, line)) return;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto fields = splitCSVLine(line);
        if (fields.size() >= 9) {
            SOTASummit s;
            std::strncpy(s.reference, fields[0].c_str(), sizeof(s.reference) - 1);
            s.reference[sizeof(s.reference) - 1] = '\0';
            s.lat = StringUtils::safe_stof(fields[7]);
            s.lon = StringUtils::safe_stof(fields[8]);
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
