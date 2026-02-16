#include "BrightnessManager.h"
#include "Logger.h"

#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <unistd.h>

BrightnessManager::BrightnessManager() {}

bool BrightnessManager::init() {
  if (!detectBrightnessPath()) {
    LOG_W("Brightness", "No brightness control found");
    available_ = false;
    return false;
  }

  // Read max brightness
  std::ifstream maxFile(maxBrightnessPath_);
  if (maxFile.is_open()) {
    maxFile >> maxBrightness_;
    maxFile.close();
    LOG_I("Brightness", "Max brightness: {}", maxBrightness_);
  }

  // Read current brightness
  int current = readBrightness();
  if (current >= 0) {
    currentPercent_ = (current * 100) / maxBrightness_;
    LOG_I("Brightness", "Current brightness: {}% (path: {})", currentPercent_,
          brightnessPath_);
    available_ = true;
    return true;
  }

  available_ = false;
  return false;
}

bool BrightnessManager::detectBrightnessPath() {
  // Common brightness control paths (RPi, x86 laptops, DSI displays)
  const char *paths[] = {
      "/sys/class/backlight/rpi_backlight/brightness",
      "/sys/class/backlight/10-0045/brightness",
      "/sys/class/backlight/6-0045/brightness",
      "/sys/class/backlight/intel_backlight/brightness",
      "/sys/class/backlight/acpi_video0/brightness",
      nullptr};

  for (int i = 0; paths[i] != nullptr; i++) {
    if (access(paths[i], W_OK) == 0) {
      brightnessPath_ = paths[i];

      // Construct max_brightness path
      std::string dir = brightnessPath_.substr(0, brightnessPath_.rfind('/'));
      maxBrightnessPath_ = dir + "/max_brightness";

      LOG_I("Brightness", "Detected control: {}", brightnessPath_);
      return true;
    }
  }

  return false;
}

bool BrightnessManager::setBrightness(int percent) {
  if (!available_) {
    return false;
  }

  // Clamp to valid range
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;

  // Convert percentage to hardware value
  int value = (percent * maxBrightness_) / 100;

  if (writeBrightness(value)) {
    currentPercent_ = percent;
    LOG_I("Brightness", "Set to {}% ({})", percent, value);
    return true;
  }

  return false;
}

int BrightnessManager::getBrightness() const {
  if (!available_) {
    return -1;
  }

  return currentPercent_;
}

void BrightnessManager::setDimTime(int hour, int minute) {
  dimHour_ = hour;
  dimMinute_ = minute;
  LOG_I("Brightness", "Dim time set to {:02d}:{:02d}", hour, minute);
}

void BrightnessManager::setBrightTime(int hour, int minute) {
  brightHour_ = hour;
  brightMinute_ = minute;
  LOG_I("Brightness", "Bright time set to {:02d}:{:02d}", hour, minute);
}

void BrightnessManager::update() {
  if (!available_ || !scheduleEnabled_) {
    return;
  }

  bool shouldDim = shouldBeDimmed();
  int targetPercent = shouldDim ? dimLevel_ : 100;

  // Only change if different from current
  if (targetPercent != currentPercent_) {
    setBrightness(targetPercent);
  }
}

bool BrightnessManager::shouldBeDimmed() const {
  auto now = std::chrono::system_clock::now();
  std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm localTm;
  localtime_r(&nowTime, &localTm);

  int currentMinutes = localTm.tm_hour * 60 + localTm.tm_min;
  int dimMinutes = dimHour_ * 60 + dimMinute_;
  int brightMinutes = brightHour_ * 60 + brightMinute_;

  // Handle wrap-around (e.g., dim at 22:00, bright at 06:00)
  if (dimMinutes > brightMinutes) {
    // Dim period crosses midnight
    return (currentMinutes >= dimMinutes || currentMinutes < brightMinutes);
  } else {
    // Dim period within same day (unusual, but handle it)
    return (currentMinutes >= dimMinutes && currentMinutes < brightMinutes);
  }
}

bool BrightnessManager::writeBrightness(int value) {
  std::ofstream file(brightnessPath_);
  if (!file.is_open()) {
    LOG_E("Brightness", "Failed to open {} for writing", brightnessPath_);
    return false;
  }

  file << value;
  file.close();

  if (file.fail()) {
    LOG_E("Brightness", "Failed to write brightness value");
    return false;
  }

  return true;
}

int BrightnessManager::readBrightness() const {
  std::ifstream file(brightnessPath_);
  if (!file.is_open()) {
    LOG_E("Brightness", "Failed to open {} for reading", brightnessPath_);
    return -1;
  }

  int value;
  file >> value;
  file.close();

  if (file.fail()) {
    LOG_E("Brightness", "Failed to read brightness value");
    return -1;
  }

  return value;
}
