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
    XSET_DPMS,  // xset dpms force off/on (X11 DPMS)
    TVSERVICE,  // tvservice -o/-p (legacy RPi)
    WLR_RANDR,  // wlr-randr --output ... --off/--on (Wayland)
    DRM_DPMS,   // Direct DRM DPMS (KMSDRM)
    CONSOLE,    // Linux VT Console blanking
    NONE
  };

  DisplayPower();
  ~DisplayPower() = default;

  void init(); // Detect available method
  bool setPower(bool on);
  bool getPower() const;
  // Render loop calls this: returns true once after power-off to push one
  // black frame, then false so SDL_RenderPresent stops and KMS goes idle.
  bool consumeBlackFrame();
  const std::vector<Method> &getMethods() const { return methods_; }
  std::string getMethodName() const;
  std::string getSelectedMethodName() const { return methodToString(selectedMethod_); }
  void setMethodByName(const std::string &name) { selectedMethod_ = stringToMethod(name); }
  void excludeMethod(Method m);
  void setDrmFd(int fd);

  static std::string methodToString(Method m);
  static Method stringToMethod(const std::string &s);

private:
  std::vector<Method> methods_;
  Method selectedMethod_ = Method::NONE;
  std::string blPowerPath_;
  bool currentPower_ = true;
  bool needsBlackFrame_ = false;
  int drmFd_ = -1;

  std::string findBacklightPowerPath();
  bool writeSysfs(const std::string &path, const std::string &value);
  bool runVcgencmd(bool on);
  bool blankFramebuffer(bool blank);
  bool runXsetDpms(bool on);
  bool runTvservice(bool on);
  bool runWlrRandr(bool on);
  bool runDrmDpms(bool on);
  bool runConsoleBlank(bool on);
  static bool binaryExists(const char *name);
  std::string xDisplayEnv_;
  std::string wlDisplayEnv_;
  std::string wlrRandrOutput_;
};
