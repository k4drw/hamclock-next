#pragma once

#include <string>

// Monitors CPU temperature from thermal zones
// Reads from /sys/class/thermal/thermal_zone*/temp
class CPUMonitor {
public:
  CPUMonitor();
  ~CPUMonitor() = default;

  // Initialize and detect thermal zone
  bool init();

  // Get current CPU temperature in Celsius
  float getTemperature() const;

  // Get temperature in Fahrenheit
  float getTemperatureF() const {
    return (getTemperature() * 9.0f / 5.0f) + 32.0f;
  }

  // Check if temperature reading is available
  bool isAvailable() const { return available_; }

  // Get thermal zone path being used
  std::string getPath() const { return thermalPath_; }

private:
  bool detectThermalZone();
  float readTemperature() const;

  std::string thermalPath_;
  bool available_ = false;
};
