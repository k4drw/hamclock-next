#pragma once

#include "../core/WeatherData.h"
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>

class BME280Provider {
public:
  BME280Provider(std::shared_ptr<WeatherStore> store);
  ~BME280Provider();

  void start();
  void stop();

  bool isAvailable() const { return available_; }

private:
  void worker();
  bool initI2C();
  bool readCalibration();
  bool readSensor();

  std::shared_ptr<WeatherStore> store_;
  std::atomic<bool> running_{false};
  std::atomic<bool> available_{false};
  std::mutex stopMutex_;
  std::condition_variable stopCv_;
  std::thread thread_;
  int fd_ = -1;
  uint8_t addr_ = 0x76; // Default BME280 address
  int32_t t_fine_ = 0;  // Shared intermediate used by pressure/humidity compensation

  // Calibration data
  struct CalibData {
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t dig_H1, dig_H3;
    int16_t dig_H2, dig_H4, dig_H5;
    int8_t dig_H6;
  } calib_;
};
