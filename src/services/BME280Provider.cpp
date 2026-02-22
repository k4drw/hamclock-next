#include "BME280Provider.h"
#include "../core/Logger.h"
#include <chrono>
#include <cmath>
#include <cstring>

#ifdef __linux__
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

BME280Provider::BME280Provider(std::shared_ptr<WeatherStore> store)
    : store_(std::move(store)) {}

BME280Provider::~BME280Provider() { stop(); }

void BME280Provider::start() {
  if (running_)
    return;
  running_ = true;
  thread_ = std::thread(&BME280Provider::worker, this);
}

void BME280Provider::stop() {
  running_ = false;
  if (thread_.joinable())
    thread_.join();
  if (fd_ >= 0) {
#ifdef __linux__
    close(fd_);
#endif
    fd_ = -1;
  }
}

void BME280Provider::worker() {
  if (!initI2C()) {
    LOG_W("BME280", "Sensor not found on I2C bus");
    available_ = false;
    running_ = false;
    return;
  }

  LOG_I("BME280", "Sensor initialized at address 0x{:02x}", addr_);
  available_ = true;

  while (running_) {
    if (readSensor()) {
      // Success
    } else {
      LOG_W("BME280", "Read failed");
    }
    // Sleep 60 seconds
    for (int i = 0; i < 60 && running_; ++i) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

bool BME280Provider::initI2C() {
#ifdef __linux__
  const char *dev = "/dev/i2c-1";
  fd_ = open(dev, O_RDWR);
  if (fd_ < 0) {
    // Try i2c-0? No, usually 1 on RPi.
    return false;
  }

  // Try 0x76 then 0x77
  addr_ = 0x76;
  if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
    addr_ = 0x77;
    if (ioctl(fd_, I2C_SLAVE, addr_) < 0) {
      close(fd_);
      fd_ = -1;
      return false;
    }
  }

  // Check ID
  uint8_t id_reg = 0xD0;
  if (write(fd_, &id_reg, 1) != 1) return false;
  uint8_t id;
  if (read(fd_, &id, 1) != 1) return false;

  if (id != 0x60) { // BME280 ID
    // Maybe BMP280 (0x58)?
    if (id != 0x58) {
      LOG_D("BME280", "Unknown chip ID: 0x{:02x}", id);
      // close(fd_); fd_ = -1; return false; // Allow for now, might be compatible
    }
  }

  // Read calibration registers before configuring the sensor
  if (!readCalibration()) {
    LOG_W("BME280", "Failed to read calibration data");
    close(fd_);
    fd_ = -1;
    return false;
  }

  // Configure: Normal mode, 16x oversampling, filter 16
  uint8_t config[] = {0xF5, 0b10010000}; // Config: 500ms, filter 16
  write(fd_, config, 2);
  uint8_t ctrl[] = {0xF4, 0b01010111}; // Ctrl: x2, x16, Normal
  write(fd_, ctrl, 2);

  return true;
#else
  return false;
#endif
}

bool BME280Provider::readCalibration() {
#ifdef __linux__
  // Bank 1: 0x88..0x9F (24 bytes) — T1-T3, P1-P9
  uint8_t reg1 = 0x88;
  if (write(fd_, &reg1, 1) != 1) return false;
  uint8_t b1[24];
  if (read(fd_, b1, 24) != 24) return false;

  calib_.dig_T1 = (uint16_t)(b1[1]  << 8) | b1[0];
  calib_.dig_T2 = (int16_t) ((b1[3]  << 8) | b1[2]);
  calib_.dig_T3 = (int16_t) ((b1[5]  << 8) | b1[4]);
  calib_.dig_P1 = (uint16_t)(b1[7]  << 8) | b1[6];
  calib_.dig_P2 = (int16_t) ((b1[9]  << 8) | b1[8]);
  calib_.dig_P3 = (int16_t) ((b1[11] << 8) | b1[10]);
  calib_.dig_P4 = (int16_t) ((b1[13] << 8) | b1[12]);
  calib_.dig_P5 = (int16_t) ((b1[15] << 8) | b1[14]);
  calib_.dig_P6 = (int16_t) ((b1[17] << 8) | b1[16]);
  calib_.dig_P7 = (int16_t) ((b1[19] << 8) | b1[18]);
  calib_.dig_P8 = (int16_t) ((b1[21] << 8) | b1[20]);
  calib_.dig_P9 = (int16_t) ((b1[23] << 8) | b1[22]);

  // H1: single byte at 0xA1
  uint8_t reg_h1 = 0xA1;
  if (write(fd_, &reg_h1, 1) != 1) return false;
  if (read(fd_, &calib_.dig_H1, 1) != 1) return false;

  // Bank 2: 0xE1..0xE7 (7 bytes) — H2-H6
  uint8_t reg2 = 0xE1;
  if (write(fd_, &reg2, 1) != 1) return false;
  uint8_t b2[7];
  if (read(fd_, b2, 7) != 7) return false;

  calib_.dig_H2 = (int16_t)((b2[1] << 8) | b2[0]);
  calib_.dig_H3 = b2[2];
  // H4 and H5 share register 0xE5 — split-byte layout per Bosch datasheet §4.2.2
  calib_.dig_H4 = (int16_t)((b2[3] << 4) | (b2[4] & 0x0F));
  calib_.dig_H5 = (int16_t)((b2[5] << 4) | (b2[4] >> 4));
  calib_.dig_H6 = (int8_t)b2[6];

  return true;
#else
  return false;
#endif
}

bool BME280Provider::readSensor() {
#ifdef __linux__
  if (fd_ < 0) return false;

  // Read registers 0xF7 to 0xFE (8 bytes)
  uint8_t reg = 0xF7;
  if (write(fd_, &reg, 1) != 1) return false;
  
  uint8_t data[8];
  if (read(fd_, data, 8) != 8) return false;

  // Raw ADC values from burst read
  int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4) | (data[2] >> 4);
  int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4) | (data[5] >> 4);
  int32_t adc_H = ((int32_t)data[6] << 8)  |  data[7];

  // --- Temperature (Bosch datasheet §4.2.3, 32-bit integer version) ---
  // Produces t_fine_ shared by pressure and humidity compensation.
  int32_t var1 = (((adc_T >> 3) - ((int32_t)calib_.dig_T1 << 1)) *
                  (int32_t)calib_.dig_T2) >> 11;
  int32_t var2 = (((((adc_T >> 4) - (int32_t)calib_.dig_T1) *
                    ((adc_T >> 4) - (int32_t)calib_.dig_T1)) >> 12) *
                  (int32_t)calib_.dig_T3) >> 14;
  t_fine_ = var1 + var2;
  // Result in hundredths of °C (e.g. 5123 = 51.23 °C)
  float tempC = (float)((t_fine_ * 5 + 128) >> 8) / 100.0f;

  // --- Pressure (Bosch datasheet §4.2.3, 64-bit integer version) ---
  int64_t p1 = (int64_t)t_fine_ - 128000;
  int64_t p2 = p1 * p1 * (int64_t)calib_.dig_P6;
  p2 += (p1 * (int64_t)calib_.dig_P5) << 17;
  p2 += (int64_t)calib_.dig_P4 << 35;
  p1 = ((p1 * p1 * (int64_t)calib_.dig_P3) >> 8) +
       ((p1 * (int64_t)calib_.dig_P2) << 12);
  p1 = ((((int64_t)1 << 47) + p1) * (int64_t)calib_.dig_P1) >> 33;
  float pressHpa = 0.0f;
  if (p1 != 0) {
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = ((int64_t)calib_.dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    p2 = ((int64_t)calib_.dig_P8 * p) >> 19;
    p = ((p + p1 + p2) >> 8) + ((int64_t)calib_.dig_P7 << 4);
    // p is in Pa * 256 (Q24.8 format)
    pressHpa = (float)p / 256.0f / 100.0f;
  }

  // --- Humidity (Bosch datasheet §4.2.3, 32-bit integer version) ---
  int32_t h = t_fine_ - 76800;
  h = (((((adc_H << 14) -
          ((int32_t)calib_.dig_H4 << 20) -
          ((int32_t)calib_.dig_H5 * h)) + 16384) >> 15) *
       (((((((h * (int32_t)calib_.dig_H6) >> 10) *
            (((h * (int32_t)calib_.dig_H3) >> 11) + 32768)) >> 10) +
          2097152) * (int32_t)calib_.dig_H2 + 8192) >> 14));
  h -= (((((h >> 15) * (h >> 15)) >> 7) * (int32_t)calib_.dig_H1) >> 4);
  h = (h < 0) ? 0 : h;
  h = (h > 419430400) ? 419430400 : h;
  // Result in Q22.10 format — divide by 1024 for %rH
  float humPct = (float)(h >> 12) / 1024.0f;

  WeatherData wd;
  wd.temp        = tempC;
  wd.pressure    = pressHpa;
  wd.humidity    = static_cast<int>(humPct + 0.5f);
  wd.description = "BME280";
  wd.valid       = true;
  wd.lastUpdate  = std::chrono::system_clock::now();
  store_->update(wd);

  LOG_D("BME280", "T={:.1f}C P={:.1f}hPa H={}%", tempC, pressHpa, wd.humidity);
  return true;
#else
  return false;
#endif
}
