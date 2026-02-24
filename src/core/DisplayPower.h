#pragma once

#include <string>
#include <vector>

class DisplayPower {
public:
  enum class Method {
    VCGENCMD,    // vcgencmd display_power (RPi preferred)
    BL_POWER,    // /sys/class/backlight/*/bl_power (DSI)
    FRAMEBUFFER, // Write black to /dev/fb0 (visual fallback)
    SOFTWARE,    // Software-controlled blanking (e.g., SDL)
    NONE
  };

  DisplayPower();
  ~DisplayPower() = default;

  void init(); // Detect available method
  bool setPower(bool on);
  bool getPower() const;
  const std::vector<Method> &getMethods() const { return methods_; }
  std::string getMethodName() const;

private:
  std::vector<Method> methods_;
  std::string blPowerPath_;
  bool currentPower_ = true;

  std::string findBacklightPowerPath();
  bool writeSysfs(const std::string &path, const std::string &value);
  bool runVcgencmd(bool on);
  bool blankFramebuffer(bool blank);
};
