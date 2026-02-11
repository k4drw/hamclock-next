#pragma once

#include <cstdio>
#include <string>

enum class WidgetType {
    SOLAR,
    DX_CLUSTER,
    LIVE_SPOTS,
    BAND_CONDITIONS,
    CONTESTS,
    ON_THE_AIR,
};

inline const char* widgetTypeToString(WidgetType t) {
    switch (t) {
    case WidgetType::SOLAR:           return "solar";
    case WidgetType::DX_CLUSTER:      return "dx_cluster";
    case WidgetType::LIVE_SPOTS:      return "live_spots";
    case WidgetType::BAND_CONDITIONS: return "band_conditions";
    case WidgetType::CONTESTS:        return "contests";
    case WidgetType::ON_THE_AIR:      return "on_the_air";
    }
    return "solar";
}

inline const char* widgetTypeDisplayName(WidgetType t) {
    switch (t) {
    case WidgetType::SOLAR:           return "Solar";
    case WidgetType::DX_CLUSTER:      return "DX Cluster";
    case WidgetType::LIVE_SPOTS:      return "Live Spots";
    case WidgetType::BAND_CONDITIONS: return "Band Cond";
    case WidgetType::CONTESTS:        return "Contests";
    case WidgetType::ON_THE_AIR:      return "On The Air";
    }
    return "Solar";
}

inline WidgetType widgetTypeFromString(const std::string& s, WidgetType fallback) {
    if (s == "solar")           return WidgetType::SOLAR;
    if (s == "dx_cluster")      return WidgetType::DX_CLUSTER;
    if (s == "live_spots")      return WidgetType::LIVE_SPOTS;
    if (s == "band_conditions") return WidgetType::BAND_CONDITIONS;
    if (s == "contests")        return WidgetType::CONTESTS;
    if (s == "on_the_air")      return WidgetType::ON_THE_AIR;
    std::fprintf(stderr, "WidgetType: unknown '%s', using fallback\n", s.c_str());
    return fallback;
}
