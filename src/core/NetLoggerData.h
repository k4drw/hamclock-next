#pragma once

#include <string>
#include <vector>
#include <ctime>

struct NetLoggerCheckin {
    int serialNo;
    std::string callsign;
    std::string state;
    std::string grid;
    std::string remarks;
    std::string cityCountry;
    std::string firstName;
    std::string status;
    std::string county;
    
    // Derived
    double lat = 0.0;
    double lon = 0.0;
    bool hasLocation = false;
};

struct NetLoggerNet {
    std::string serverName;
    std::string netName;
    std::string frequency;
    std::string logger;
    std::string netControl;
    std::string date;
    std::string mode;
    std::string band;
    int subscriberCount = 0;
};

struct NetLoggerData {
    bool valid = false;
    time_t lastUpdate = 0;
    std::vector<NetLoggerNet> activeNets;
    
    bool hasCheckins = false;
    std::string selectedNetName;
    std::string selectedServerName;
    std::vector<NetLoggerCheckin> checkins;
    int checkinCount = 0;
};
