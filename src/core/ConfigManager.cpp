#include "ConfigManager.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>

static std::string colorToHex(SDL_Color c)
{
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

static SDL_Color hexToColor(const std::string& hex, SDL_Color fallback)
{
    if (hex.size() < 7 || hex[0] != '#') return fallback;
    unsigned int r = 0, g = 0, b = 0;
    if (std::sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) == 3) {
        return {static_cast<Uint8>(r), static_cast<Uint8>(g),
                static_cast<Uint8>(b), 255};
    }
    return fallback;
}

bool ConfigManager::init()
{
    // XDG_CONFIG_HOME or fallback to ~/.config
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        configDir_ = std::filesystem::path(xdg) / "hamclock";
    } else {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') {
            std::fprintf(stderr, "ConfigManager: HOME not set\n");
            return false;
        }
        configDir_ = std::filesystem::path(home) / ".config" / "hamclock";
    }

    configPath_ = configDir_ / "config.json";
    return true;
}

bool ConfigManager::load(AppConfig& config) const
{
    if (configPath_.empty()) return false;

    std::ifstream ifs(configPath_);
    if (!ifs) return false;

    auto json = nlohmann::json::parse(ifs, nullptr, false);
    if (json.is_discarded()) {
        std::fprintf(stderr, "ConfigManager: invalid JSON in %s\n",
                     configPath_.c_str());
        return false;
    }

    // Identity
    if (json.contains("identity")) {
        auto& id = json["identity"];
        config.callsign = id.value("callsign", "");
        config.grid     = id.value("grid", "");
        config.lat      = id.value("lat", 0.0);
        config.lon      = id.value("lon", 0.0);
    }

    // Appearance
    if (json.contains("appearance")) {
        auto& ap = json["appearance"];
        std::string hexColor = ap.value("callsign_color", "");
        if (!hexColor.empty()) {
            config.callsignColor = hexToColor(hexColor, config.callsignColor);
        }
        config.theme = ap.value("theme", "default");
    }

    // Pane widget selection
    if (json.contains("panes")) {
        auto& pa = json["panes"];
        config.pane1Widget = widgetTypeFromString(
            pa.value("pane1_widget", "solar"), WidgetType::SOLAR);
        config.pane2Widget = widgetTypeFromString(
            pa.value("pane2_widget", "dx_cluster"), WidgetType::DX_CLUSTER);
        config.pane3Widget = widgetTypeFromString(
            pa.value("pane3_widget", "live_spots"), WidgetType::LIVE_SPOTS);
    }

    // Panel state
    if (json.contains("panel")) {
        auto& pn = json["panel"];
        config.panelMode          = pn.value("mode", "dx");
        config.selectedSatellite  = pn.value("satellite", "");
    }

    // Require at least a callsign to consider config valid
    return !config.callsign.empty();
}

bool ConfigManager::save(const AppConfig& config) const
{
    if (configPath_.empty()) return false;

    // Create directory if needed
    std::error_code ec;
    std::filesystem::create_directories(configDir_, ec);
    if (ec) {
        std::fprintf(stderr, "ConfigManager: cannot create %s: %s\n",
                     configDir_.c_str(), ec.message().c_str());
        return false;
    }

    nlohmann::json json;
    json["identity"]["callsign"] = config.callsign;
    json["identity"]["grid"]     = config.grid;
    json["identity"]["lat"]      = config.lat;
    json["identity"]["lon"]      = config.lon;

    json["appearance"]["callsign_color"] = colorToHex(config.callsignColor);
    json["appearance"]["theme"]          = config.theme;

    json["panes"]["pane1_widget"] = widgetTypeToString(config.pane1Widget);
    json["panes"]["pane2_widget"] = widgetTypeToString(config.pane2Widget);
    json["panes"]["pane3_widget"] = widgetTypeToString(config.pane3Widget);

    json["panel"]["mode"]      = config.panelMode;
    json["panel"]["satellite"] = config.selectedSatellite;

    std::ofstream ofs(configPath_);
    if (!ofs) {
        std::fprintf(stderr, "ConfigManager: cannot write %s\n",
                     configPath_.c_str());
        return false;
    }

    ofs << json.dump(2) << "\n";
    return ofs.good();
}
