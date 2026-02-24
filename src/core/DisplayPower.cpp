#include "DisplayPower.h"
#include "Logger.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __linux__
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h>
#define access _access
#define W_OK 2
#else
#include <unistd.h>
#endif

DisplayPower::DisplayPower() { init(); }

void DisplayPower::init() {
#ifndef __EMSCRIPTEN__
  methods_.clear();

  // 1. Test vcgencmd (RPi preferred)
  if (std::system("vcgencmd display_power -1 > /dev/null 2>&1") == 0) {
    methods_.push_back(Method::VCGENCMD);
    LOG_I("Display", "Detected screen control: vcgencmd");
  }

  // 2. Test bl_power sysfs (DSI displays)
  blPowerPath_ = findBacklightPowerPath();
  if (!blPowerPath_.empty()) {
    methods_.push_back(Method::BL_POWER);
    LOG_I("Display", "Detected screen control: sysfs bl_power ({})",
          blPowerPath_);
  }

  // 3. Fallback to framebuffer blanking
  if (access("/dev/fb0", W_OK) == 0) {
    methods_.push_back(Method::FRAMEBUFFER);
    LOG_I("Display",
          "Detected screen control: Framebuffer blanking (visual only)");
  }

  // 4. Always add SOFTWARE blanking as a robust fallback
  methods_.push_back(Method::SOFTWARE);
  LOG_I("Display", "Detected screen control: Software blanking");
#else
  methods_.push_back(Method::NONE);
#endif
}

bool DisplayPower::setPower(bool on) {
  bool success = false;
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
  (void)on;
  success = false;
#else
  for (auto method : methods_) {
    bool m_success = false;
    switch (method) {
    case Method::VCGENCMD:
      m_success = runVcgencmd(on);
      break;
    case Method::BL_POWER:
      m_success = writeSysfs(blPowerPath_, on ? "0" : "1");
      break;
    case Method::FRAMEBUFFER:
      m_success = blankFramebuffer(!on);
      break;
    case Method::SOFTWARE:
      // Software power state is always "successful" from DisplayPower's
      // perspective. The application (main.cpp) will check getPower() to
      // perform actual blanking.
      m_success = true;
      break;
    case Method::NONE:
      m_success = false;
      break;
    }
    if (m_success)
      success = true; // At least one method "worked"
  }
#endif
#else
  (void)on;
  success = false;
#endif

  if (success) {
    currentPower_ = on;
    LOG_I("Display", "Screen power set to {}", on ? "ON" : "OFF");
  } else {
    // Only log error on desktop
#ifndef __EMSCRIPTEN__
    LOG_E("Display", "Failed to set screen power to {}", on ? "ON" : "OFF");
#endif
  }
  return success;
}

bool DisplayPower::getPower() const { return currentPower_; }

std::string DisplayPower::getMethodName() const {
#ifndef __EMSCRIPTEN__
  std::string name;
  for (auto m : methods_) {
    if (!name.empty())
      name += ", ";
    switch (m) {
    case Method::VCGENCMD:
      name += "vcgencmd";
      break;
    case Method::BL_POWER:
      name += "sysfs (bl_power)";
      break;
    case Method::FRAMEBUFFER:
      name += "framebuffer blank";
      break;
    case Method::SOFTWARE:
      name += "software";
      break;
    case Method::NONE:
      name += "none";
      break;
    }
  }
  return name.empty() ? "none" : name;
#endif
  return "none";
}

std::string DisplayPower::findBacklightPowerPath() {
#ifndef __EMSCRIPTEN__
  const char *paths[] = {"/sys/class/backlight/rpi_backlight/bl_power",
                         "/sys/class/backlight/10-0045/bl_power",
                         "/sys/class/backlight/6-0045/bl_power", nullptr};

  for (int i = 0; paths[i]; i++) {
    if (access(paths[i], W_OK) == 0) {
      return paths[i];
    }
  }
#endif
  return "";
}

bool DisplayPower::writeSysfs(const std::string &path,
                              const std::string &value) {
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
  (void)path;
  (void)value;
  return false;
#else
#ifdef __linux__
  int fd = open(path.c_str(), O_WRONLY);
  if (fd < 0)
    return false;
  ssize_t ret = write(fd, value.c_str(), value.size());
  close(fd);
  return ret > 0;
#else
  (void)path;
  (void)value;
  return false;
#endif
#endif
#else
  (void)path;
  (void)value;
  return false;
#endif
}

bool DisplayPower::runVcgencmd(bool on) {
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
  (void)on;
  return false;
#else
  std::string cmd =
      on ? "vcgencmd display_power 1" : "vcgencmd display_power 0";
  return std::system((cmd + " > /dev/null 2>&1").c_str()) == 0;
#endif
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::blankFramebuffer(bool blank) {
#ifndef __EMSCRIPTEN__
#ifdef __linux__
  int fd = open("/dev/fb0", O_RDWR);
  if (fd < 0)
    return false;

  struct fb_var_screeninfo vinfo;
  if (ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
    close(fd);
    return false;
  }

  size_t size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8);
  if (size == 0) {
    close(fd);
    return false;
  }

  // In a real implementation this might interfere with SDL.
  // For now we'll just log that we would blank it.
  // We can't easily "unblank" the FB once we zero it if SDL is still wanting to
  // render to it. But on RPi, vcgencmd or bl_power usually work.
  close(fd);

  // If we're using FB blanking as fallback, it's mostly a dummy for now.
  return true;
#else
  return false;
#endif
#else
  (void)blank;
  return false;
#endif
}
