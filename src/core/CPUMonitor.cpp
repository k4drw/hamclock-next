#include "CPUMonitor.h"
#include "Logger.h"

#include <fstream>

#if defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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
#if defined(_WIN32)
    (void)i; // no thermal zones on Windows — skip loop body
    break;
#else
    if (access(paths[i], R_OK) == 0) {
      thermalPath_ = paths[i];
      LOG_I("CPUMonitor", "Detected thermal zone: {}", thermalPath_);
      return true;
    }
#endif
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

// ---------------------------------------------------------------------------
// getCpuPercent() — cross-platform CPU utilisation (0–100%)
// All branches compute an idle-vs-total delta since the previous call.
// ---------------------------------------------------------------------------
float CPUMonitor::getCpuPercent() {
#if defined(__linux__)
  // /proc/stat aggregate "cpu" line:
  //   cpu user nice system idle iowait irq softirq steal ...
  // Idle = idle + iowait;  Total = sum of all fields
  FILE *fp = fopen("/proc/stat", "r");
  if (!fp)
    return 0.0f;

  long long user = 0, nice = 0, sys = 0, idle = 0, iowait = 0, irq = 0,
            softirq = 0, steal = 0;
  int n = fscanf(fp, "cpu %lld %lld %lld %lld %lld %lld %lld %lld", &user,
                 &nice, &sys, &idle, &iowait, &irq, &softirq, &steal);
  fclose(fp);
  if (n < 4)
    return 0.0f;

  long long idleNow = idle + iowait;
  long long totalNow =
      user + nice + sys + idle + iowait + irq + softirq + steal;

#elif defined(__APPLE__)
  // host_statistics64 HOST_CPU_LOAD_INFO: per-state tick totals (all cores)
  host_cpu_load_info_data_t ci;
  mach_msg_type_number_t cnt = HOST_CPU_LOAD_INFO_COUNT;
  if (host_statistics64(mach_host_self(), HOST_CPU_LOAD_INFO,
                        reinterpret_cast<host_info64_t>(&ci),
                        &cnt) != KERN_SUCCESS) {
    return 0.0f;
  }

  long long idleNow = ci.cpu_ticks[CPU_STATE_IDLE];
  long long totalNow =
      ci.cpu_ticks[CPU_STATE_USER] + ci.cpu_ticks[CPU_STATE_SYSTEM] +
      ci.cpu_ticks[CPU_STATE_IDLE] + ci.cpu_ticks[CPU_STATE_NICE];

#elif defined(_WIN32)
  // GetSystemTimes FILETIME (100ns intervals).
  // kernel includes idle; busy = (kernel - idle) + user
  FILETIME idleFT, kernelFT, userFT;
  if (!GetSystemTimes(&idleFT, &kernelFT, &userFT))
    return 0.0f;

  auto toLL = [](FILETIME ft) -> long long {
    return (static_cast<long long>(ft.dwHighDateTime) << 32) |
           static_cast<long long>(ft.dwLowDateTime);
  };
  long long idleNow = toLL(idleFT);
  long long totalNow = toLL(kernelFT) + toLL(userFT);

#else
  return 0.0f; // unsupported platform
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
  long long dI = idleNow - prevIdle_;
  long long dT = totalNow - prevTotal_;
  prevIdle_ = idleNow;
  prevTotal_ = totalNow;
  if (dT <= 0)
    return 0.0f;

  float pct = 100.0f * (1.0f - static_cast<float>(dI) / static_cast<float>(dT));
  if (pct < 0.0f)
    pct = 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  return pct;
#endif
}
