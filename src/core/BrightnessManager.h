#pragma once

#include <chrono>
#include <string>

class LTR329Provider;

// Manages display brightness via sysfs and scheduled dimming
// Ported from original HamClock brightness.cpp
class BrightnessManager {
public:
  BrightnessManager();
  ~BrightnessManager() = default;

  // Initialize and detect brightness control method
  bool init();

  // Set brightness level (0-100%)
  bool setBrightness(int percent);

  // Get current brightness level
  int getBrightness() const;

  // Enable/disable scheduled dimming
  void setScheduleEnabled(bool enabled) { scheduleEnabled_ = enabled; }
  bool isScheduleEnabled() const { return scheduleEnabled_; }

  // Set dimming schedule times (24-hour format)
  void setDimTime(int hour, int minute);
  void setBrightTime(int hour, int minute);

  int getDimHour() const { return dimHour_; }
  int getDimMinute() const { return dimMinute_; }
  int getBrightHour() const { return brightHour_; }
  int getBrightMinute() const { return brightMinute_; }

  // Set dimmed brightness level (0-100%)
  void setDimLevel(int percent) { dimLevel_ = percent; }
  int getDimLevel() const { return dimLevel_; }

  // Update brightness based on schedule (call periodically)
  void update();

  // Check if brightness control is available
  bool isAvailable() const { return available_; }

  // Get sysfs path being used
  std::string getPath() const { return brightnessPath_; }

  // Set LTR329 photosensor for auto-dim (optional; call before update())
  void setLtr329Provider(LTR329Provider *ltr) { ltr329_ = ltr; }
  void setLtr329AutoDim(bool enabled) { ltr329AutoDim_ = enabled; }
  bool isLtr329AutoDimEnabled() const { return ltr329AutoDim_; }

private:
  bool detectBrightnessPath();
  bool writeBrightness(int value);
  int readBrightness() const;
  bool shouldBeDimmed() const;

  std::string brightnessPath_;
  std::string maxBrightnessPath_;
  int maxBrightness_ = 255;
  int currentPercent_ = 100;
  bool available_ = false;

  // Scheduled dimming
  bool scheduleEnabled_ = false;
  int dimHour_ = 22;      // Default: dim at 10 PM
  int dimMinute_ = 0;
  int brightHour_ = 6;    // Default: brighten at 6 AM
  int brightMinute_ = 0;
  int dimLevel_ = 20;     // Default: 20% when dimmed

  // LTR329 photosensor auto-dim
  LTR329Provider *ltr329_ = nullptr;
  bool ltr329AutoDim_ = false;
};
