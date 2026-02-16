#pragma once

#include <string>

class DisplayPower {
public:
  enum class Method {
    VCGENCMD,    // vcgencmd display_power (RPi preferred)
    BL_POWER,    // /sys/class/backlight/*/bl_power (DSI)
    FRAMEBUFFER, // Write black to /dev/fb0 (visual fallback)
    NONE
  };

  DisplayPower();
  ~DisplayPower() = default;

  void init(); // Detect available method
  bool setPower(bool on);
  bool getPower() const;
  Method getMethod() const { return method_; }
  std::string getMethodName() const;

private:
  Method method_ = Method::NONE;
  std::string blPowerPath_;
  bool currentPower_ = true;

  std::string findBacklightPowerPath();
  bool writeSysfs(const std::string &path, const std::string &value);
  bool runVcgencmd(bool on);
  bool blankFramebuffer(bool blank);
};
