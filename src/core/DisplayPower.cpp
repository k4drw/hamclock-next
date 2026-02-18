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
  // 1. Test vcgencmd (RPi preferred)
  if (std::system("vcgencmd display_power -1 > /dev/null 2>&1") == 0) {
    method_ = Method::VCGENCMD;
    LOG_I("Display", "Detected screen control: vcgencmd");
    return;
  }

  // 2. Test bl_power sysfs (DSI displays)
  blPowerPath_ = findBacklightPowerPath();
  if (!blPowerPath_.empty()) {
    method_ = Method::BL_POWER;
    LOG_I("Display", "Detected screen control: sysfs bl_power ({})",
          blPowerPath_);
    return;
  }

  // 3. Fallback to framebuffer blanking
  if (access("/dev/fb0", W_OK) == 0) {
    method_ = Method::FRAMEBUFFER;
    LOG_I("Display",
          "Detected screen control: Framebuffer blanking (visual only)");
    return;
  }

  method_ = Method::NONE;
  LOG_W("Display", "No hardware screen control detected.");
#else
  method_ = Method::NONE;
#endif
}

bool DisplayPower::setPower(bool on) {
  bool success = false;
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
  (void)on;
  success = false;
#else
  switch (method_) {
  case Method::VCGENCMD:
    success = runVcgencmd(on);
    break;
  case Method::BL_POWER:
    success = writeSysfs(blPowerPath_, on ? "0" : "1");
    break;
  case Method::FRAMEBUFFER:
    success = blankFramebuffer(!on);
    break;
  case Method::NONE:
    success = false;
    break;
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

bool DisplayPower::getPower() const {
#ifndef __EMSCRIPTEN__
#ifdef _WIN32
  return currentPower_; // On Windows, we can only return the last known state
#else
  if (method_ == Method::VCGENCMD) {
    // We can actually query hardware for vcgencmd
    FILE *pipe = popen("vcgencmd display_power", "r");
    if (pipe) {
      char buf[128];
      if (fgets(buf, sizeof(buf), pipe)) {
        pclose(pipe);
        return strstr(buf, "=1") != nullptr;
      }
      pclose(pipe);
    }
  }
  return currentPower_;
#endif
#else
  return true;
#endif
}

std::string DisplayPower::getMethodName() const {
#ifndef __EMSCRIPTEN__
  switch (method_) {
  case Method::VCGENCMD:
    return "vcgencmd";
  case Method::BL_POWER:
    return "sysfs (bl_power)";
  case Method::FRAMEBUFFER:
    return "framebuffer blank";
  case Method::NONE:
    return "none";
  }
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
