#include "NetLoggerProvider.h"
#include "../core/NetLoggerStore.h"
#include "../network/NetworkManager.h"
#include "../core/Logger.h"
#include "../core/StringUtils.h"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cctype>

NetLoggerProvider::NetLoggerProvider(NetworkManager& net, std::shared_ptr<NetLoggerStore> store) 
    : net_(net), store_(std::move(store)) {}

NetLoggerProvider::~NetLoggerProvider() {
    alive_->store(false, std::memory_order_release);
}

std::string NetLoggerProvider::urlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << "+";
        } else {
            escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char)c) << std::nouppercase;
        }
    }
    return escaped.str();
}

void NetLoggerProvider::fetch() {
    auto now = std::chrono::steady_clock::now();
    
    std::string selServer, selNet;
    if (store_) {
        store_->getSelectedNet(selServer, selNet);
    }
    if (data_.selectedServerName != selServer || data_.selectedNetName != selNet) {
        data_.selectedServerName = selServer;
        data_.selectedNetName = selNet;
        data_.checkins.clear();
        data_.hasCheckins = false;
        lastCheckinsFetchTime_ = std::chrono::steady_clock::time_point();
    }

    // Fetch Active Nets
    auto elapsedNets = std::chrono::duration_cast<std::chrono::seconds>(now - lastNetsFetchTime_).count();
    if (elapsedNets >= NETS_FETCH_INTERVAL_S && !isFetchingNets_) {
        isFetchingNets_ = true;
        lastNetsFetchTime_ = now;
        
        auto alive = alive_;
        net_.fetchAsync(
            "https://www.netlogger.org/api/GetActiveNets.php",
            [this, alive](std::string response) {
                if (!alive->load(std::memory_order_acquire)) return;
                isFetchingNets_ = false;
                
                if (!response.empty()) {
                    parseNets(response);
                    data_.lastUpdate = std::time(nullptr);
                    if (store_) store_->update(data_);
                }
            }, 30);
    }
    
    // Fetch Checkins if a net is selected
    if (!data_.selectedNetName.empty() && !data_.selectedServerName.empty()) {
        auto elapsedCheckins = std::chrono::duration_cast<std::chrono::seconds>(now - lastCheckinsFetchTime_).count();
        if (elapsedCheckins >= CHECKINS_FETCH_INTERVAL_S && !isFetchingCheckins_) {
            isFetchingCheckins_ = true;
            lastCheckinsFetchTime_ = now;
            
            std::string url = "https://www.netlogger.org/api/GetCheckins.php?ServerName=" + 
                              urlEncode(data_.selectedServerName) + 
                              "&NetName=" + urlEncode(data_.selectedNetName);
                              
            auto alive = alive_;
            net_.fetchAsync(
                url,
                [this, alive](std::string response) {
                    if (!alive->load(std::memory_order_acquire)) return;
                    isFetchingCheckins_ = false;
                    
                    if (!response.empty()) {
                        parseCheckins(response);
                        data_.lastUpdate = std::time(nullptr);
                        if (store_) store_->update(data_);
                    }
                }, 10);
        }
    }
}

std::string NetLoggerProvider::extractTag(const std::string& xml, const std::string& tag, size_t startPos, size_t& endPos) {
    std::string openTag = "<" + tag + ">";
    std::string closeTag = "</" + tag + ">";
    
    size_t posStart = xml.find(openTag, startPos);
    if (posStart == std::string::npos) {
        endPos = std::string::npos;
        return "";
    }
    
    posStart += openTag.length();
    size_t posEnd = xml.find(closeTag, posStart);
    if (posEnd == std::string::npos) {
        endPos = std::string::npos;
        return "";
    }
    
    endPos = posEnd + closeTag.length();
    return StringUtils::unescapeHtml(xml.substr(posStart, posEnd - posStart));
}

void NetLoggerProvider::parseNets(const std::string& xml) {
    std::vector<NetLoggerNet> nets;
    size_t pos = 0;
    
    while (true) {
        size_t serverStart = xml.find("<Server>", pos);
        if (serverStart == std::string::npos) break;
        
        size_t serverEnd = xml.find("</Server>", serverStart);
        if (serverEnd == std::string::npos) break;
        
        size_t ignore;
        std::string serverName = extractTag(xml, "ServerName", serverStart, ignore);
        
        size_t netPos = serverStart;
        while (netPos < serverEnd) {
            size_t netStart = xml.find("<Net>", netPos);
            if (netStart == std::string::npos || netStart > serverEnd) break;
            
            size_t netEnd = xml.find("</Net>", netStart);
            if (netEnd == std::string::npos || netEnd > serverEnd) break;
            
            NetLoggerNet net;
            net.serverName = serverName;
            net.netName = extractTag(xml, "NetName", netStart, ignore);
            net.frequency = extractTag(xml, "Frequency", netStart, ignore);
            net.logger = extractTag(xml, "Logger", netStart, ignore);
            net.netControl = extractTag(xml, "NetControl", netStart, ignore);
            net.date = extractTag(xml, "Date", netStart, ignore);
            net.mode = extractTag(xml, "Mode", netStart, ignore);
            net.band = extractTag(xml, "Band", netStart, ignore);
            
            std::string subCountStr = extractTag(xml, "SubscriberCount", netStart, ignore);
            net.subscriberCount = StringUtils::safe_stoi(subCountStr);
            
            nets.push_back(net);
            netPos = netEnd;
        }
        pos = serverEnd;
    }
    
    data_.activeNets = nets;
    data_.valid = true;
}

void NetLoggerProvider::parseCheckins(const std::string& xml) {
    std::vector<NetLoggerCheckin> checkins;
    size_t pos = 0;
    
    size_t checkinListStart = xml.find("<CheckinList>");
    if (checkinListStart == std::string::npos) return;
    
    while (true) {
        size_t checkinStart = xml.find("<Checkin>", pos);
        if (checkinStart == std::string::npos) break;
        
        size_t checkinEnd = xml.find("</Checkin>", checkinStart);
        if (checkinEnd == std::string::npos) break;
        
        size_t ignore;
        NetLoggerCheckin c;
        std::string serialNoStr = extractTag(xml, "SerialNo", checkinStart, ignore);
        c.serialNo = StringUtils::safe_stoi(serialNoStr);
        c.callsign = extractTag(xml, "Callsign", checkinStart, ignore);
        c.state = extractTag(xml, "State", checkinStart, ignore);
        c.grid = extractTag(xml, "Grid", checkinStart, ignore);
        c.remarks = extractTag(xml, "Remarks", checkinStart, ignore);
        c.cityCountry = extractTag(xml, "CityCountry", checkinStart, ignore);
        c.firstName = extractTag(xml, "FirstName", checkinStart, ignore);
        c.status = extractTag(xml, "Status", checkinStart, ignore);
        c.county = extractTag(xml, "County", checkinStart, ignore);
        
        gridToLatLon(c.grid, c.lat, c.lon, c.hasLocation);
        
        checkins.push_back(c);
        pos = checkinEnd;
    }
    
    data_.checkins = checkins;
    data_.checkinCount = checkins.size();
    data_.hasCheckins = true;
}

void NetLoggerProvider::gridToLatLon(const std::string& grid, double& lat, double& lon, bool& hasLoc) {
    hasLoc = false;
    std::string g = StringUtils::trim(grid);
    if (g.length() < 4) return;
    
    g[0] = std::toupper(g[0]);
    g[1] = std::toupper(g[1]);
    
    if (g[0] < 'A' || g[0] > 'R' || g[1] < 'A' || g[1] > 'R') return;
    if (g[2] < '0' || g[2] > '9' || g[3] < '0' || g[3] > '9') return;
    
    lon = (g[0] - 'A') * 20.0 + (g[2] - '0') * 2.0 - 180.0 + 1.0;
    lat = (g[1] - 'A') * 10.0 + (g[3] - '0') * 1.0 - 90.0 + 0.5;
    
    if (g.length() >= 6) {
        g[4] = std::toupper(g[4]);
        g[5] = std::toupper(g[5]);
        if (g[4] >= 'A' && g[4] <= 'X' && g[5] >= 'A' && g[5] <= 'X') {
            lon += (g[4] - 'A') * (2.0 / 24.0) - 1.0 + (1.0 / 24.0);
            lat += (g[5] - 'A') * (1.0 / 24.0) - 0.5 + (1.0 / 48.0);
        }
    }
    
    hasLoc = true;
}
