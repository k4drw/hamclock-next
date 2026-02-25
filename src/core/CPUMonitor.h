#pragma once

#include <string>

// Monitors CPU temperature from thermal zones and CPU utilisation.
// Temperature: reads from /sys/class/thermal/thermal_zone*/temp
// CPU%: samples /proc/stat idle/total delta on each getCpuPercent() call
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

  // Get CPU utilisation as a percentage 0–100.
  // Computed from /proc/stat idle/total delta since the last call.
  // Returns 0.0f on non-Linux platforms.
  float getCpuPercent();

  // Check if temperature reading is available
  bool isAvailable() const { return available_; }

  // Get thermal zone path being used
  std::string getPath() const { return thermalPath_; }

private:
  bool detectThermalZone();
  float readTemperature() const;

  std::string thermalPath_;
  bool available_ = false;

  // /proc/stat delta state for getCpuPercent()
  long long prevIdle_ = 0;
  long long prevTotal_ = 0;
};
