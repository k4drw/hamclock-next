#pragma once

#include "../core/WeatherData.h"
#include "../network/NetworkManager.h"
#include <memory>
#include <string>

class WeatherProvider {
public:
  WeatherProvider(NetworkManager &net, std::shared_ptr<WeatherStore> store);

  // Fetch weather for a specific location.
  void fetch(double lat, double lon);

private:
  void processData(const std::string &body);

  NetworkManager &net_;
  std::shared_ptr<WeatherStore> store_;

  // Using open-meteo.com for free, no-key weather data
  // Example:
  // https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,rain,showers,snowfall,weather_code,cloud_cover,pressure_msl,surface_pressure,wind_speed_10m,wind_direction_10m,wind_gusts_10m
};
