#include "CPUMonitor.h"
#include "Logger.h"

#include <fstream>
#include <unistd.h>

CPUMonitor::CPUMonitor() {}

bool CPUMonitor::init() {
  if (!detectThermalZone()) {
    LOG_W("CPUMonitor", "No thermal zone found");
    available_ = false;
    return false;
  }

  // Test read
  float temp = readTemperature();
  if (temp > 0.0f && temp < 150.0f) {
    LOG_I("CPUMonitor", "CPU temperature: {:.1f}°C (path: {})", temp,
          thermalPath_);
    available_ = true;
    return true;
  }

  LOG_W("CPUMonitor", "Invalid temperature reading: {:.1f}°C", temp);
  available_ = false;
  return false;
}

bool CPUMonitor::detectThermalZone() {
  // Try common thermal zones
  // thermal_zone0 is usually CPU on RPi and x86
  const char *paths[] = {"/sys/class/thermal/thermal_zone0/temp",
                         "/sys/class/thermal/thermal_zone1/temp",
                         "/sys/class/thermal/thermal_zone2/temp", nullptr};

  for (int i = 0; paths[i] != nullptr; i++) {
    if (access(paths[i], R_OK) == 0) {
      thermalPath_ = paths[i];
      LOG_I("CPUMonitor", "Detected thermal zone: {}", thermalPath_);
      return true;
    }
  }

  return false;
}

float CPUMonitor::getTemperature() const {
  if (!available_) {
    return -1.0f;
  }

  return readTemperature();
}

float CPUMonitor::readTemperature() const {
  std::ifstream file(thermalPath_);
  if (!file.is_open()) {
    LOG_E("CPUMonitor", "Failed to open thermal zone: {}", thermalPath_);
    return -1.0f;
  }

  int millidegrees;
  file >> millidegrees;
  file.close();

  if (file.fail()) {
    LOG_E("CPUMonitor", "Failed to read temperature");
    return -1.0f;
  }

  // Convert millidegrees to degrees Celsius
  return millidegrees / 1000.0f;
}
