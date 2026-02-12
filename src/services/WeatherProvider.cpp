#include "WeatherProvider.h"
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WeatherProvider::WeatherProvider(NetworkManager &net,
                                 std::shared_ptr<WeatherStore> store)
    : net_(net), store_(std::move(store)) {}

void WeatherProvider::fetch(double lat, double lon) {
  char url[256];
  std::snprintf(url, sizeof(url),
                "https://api.open-meteo.com/v1/"
                "forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,"
                "relative_humidity_2m,surface_pressure,wind_speed_10m,wind_"
                "direction_10m,weather_code",
                lat, lon);

  net_.fetchAsync(url, [this](std::string body) {
    if (!body.empty()) {
      processData(body);
    }
  });
}

// WMO Weather interpretation codes (WW)
// https://open-meteo.com/en/docs
std::string weatherCodeToDescription(int code) {
  switch (code) {
  case 0:
    return "Clear sky";
  case 1:
  case 2:
  case 3:
    return "Partly cloudy";
  case 45:
  case 48:
    return "Fog";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 56:
  case 57:
    return "Freezing Drizzle";
  case 61:
  case 63:
  case 65:
    return "Rain";
  case 66:
  case 67:
    return "Freezing Rain";
  case 71:
  case 73:
  case 75:
    return "Snow fall";
  case 77:
    return "Snow grains";
  case 80:
  case 81:
  case 82:
    return "Rain showers";
  case 85:
  case 86:
    return "Snow showers";
  case 95:
    return "Thunderstorm";
  case 96:
  case 99:
    return "Thunderstorm with hail";
  default:
    return "Unknown";
  }
}

void WeatherProvider::processData(const std::string &body) {
  try {
    auto j = json::parse(body);
    if (j.contains("current")) {
      auto current = j["current"];
      WeatherData data;
      data.temp = current["temperature_2m"];
      data.humidity = current["relative_humidity_2m"];
      data.pressure = current["surface_pressure"];
      data.windSpeed = current["wind_speed_10m"];
      data.windDeg = current["wind_direction_10m"];
      data.description = weatherCodeToDescription(current["weather_code"]);
      data.valid = true;
      data.lastUpdate = std::chrono::system_clock::now();
      store_->update(data);
    }
  } catch (...) {
    // Parse error
  }
}
