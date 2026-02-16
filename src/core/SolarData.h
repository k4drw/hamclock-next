#pragma once

#include <chrono>
#include <mutex>

struct SolarData {
  int sfi = 0;
  int k_index = 0;
  int a_index = 0;
  int sunspot_number = 0;
  double solar_wind_speed = 0.0;
  double solar_wind_density = 0.0;
  int bz = 0;
  int bt = 0;
  int aurora = 0;
  int dst = 0;
  int drap = 0;

  // NOAA Space Weather Scales data
  double xray_flux = 0.0;        // W/m² for R-scale calculation
  double proton_flux = 0.0;      // pfu (>=10 MeV) for S-scale calculation
  int noaa_r_scale = 0;          // R0-R5: Radio Blackout scale
  int noaa_s_scale = 0;          // S0-S5: Solar Radiation Storm scale
  int noaa_g_scale = 0;          // G0-G5: Geomagnetic Storm scale

  std::chrono::system_clock::time_point last_updated{};
  bool valid = false;
};

class SolarDataStore {
public:
  SolarData get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
  }

  void set(const SolarData &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_ = data;
  }

private:
  mutable std::mutex mutex_;
  SolarData data_;
};
