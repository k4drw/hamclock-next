#include "DisplayPower.h"
#include "Logger.h"
#include "SoundManager.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef __linux__
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/tiocl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#ifdef HAS_LIBDRM
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif
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
#if defined(__EMSCRIPTEN__)
  methods_.push_back(Method::NONE);
#else
  methods_.clear();

  // 1. DRM DPMS — preferred when available; setDrmFd() provides the master fd
  //    so this works even when SDL2 KMSDRM holds DRM master.
  if (access("/dev/dri/card0", W_OK) == 0) {
    methods_.push_back(Method::DRM_DPMS);
    LOG_I("Display", "Detected screen control: Direct DRM DPMS");
  }

  // 2. vcgencmd (RPi VideoCore firmware — works alongside KMSDRM)
  if (std::system("vcgencmd display_power -1 > /dev/null 2>&1") == 0) {
    methods_.push_back(Method::VCGENCMD);
    LOG_I("Display", "Detected screen control: vcgencmd");
  }

  // 3. bl_power sysfs (DSI displays)
  blPowerPath_ = findBacklightPowerPath();
  if (!blPowerPath_.empty()) {
    methods_.push_back(Method::BL_POWER);
    LOG_I("Display", "Detected screen control: sysfs bl_power ({})",
          blPowerPath_);
  }

  // 4. Framebuffer blanking (visual fallback)
  if (access("/dev/fb0", W_OK) == 0) {
    methods_.push_back(Method::FRAMEBUFFER);
    LOG_I("Display",
          "Detected screen control: Framebuffer blanking (visual only)");
  }

  // 5. X11 DPMS via xset
  {
    const char *disp = std::getenv("DISPLAY");
    if (disp && disp[0] && binaryExists("xset")) {
      xDisplayEnv_ = disp;
      methods_.push_back(Method::XSET_DPMS);
      LOG_I("Display", "Detected screen control: xset dpms (X11)");
    }
  }

  // 6. tvservice (legacy RPi firmware stack)
  if (binaryExists("tvservice")) {
    methods_.push_back(Method::TVSERVICE);
    LOG_I("Display", "Detected screen control: tvservice (legacy RPi)");
  }

  // 7. wlr-randr (Wayland compositors: sway, labwc, etc.)
  {
    const char *wdisp = std::getenv("WAYLAND_DISPLAY");
    if (wdisp && wdisp[0] && binaryExists("wlr-randr")) {
      wlDisplayEnv_ = wdisp;
      // Detect first output name for --off/--on
      FILE *fp = popen("wlr-randr 2>/dev/null | head -1", "r");
      if (fp) {
        char buf[128] = {};
        if (fgets(buf, sizeof(buf), fp)) {
          // Output line format: "HDMI-A-1 ..."
          char *sp = std::strchr(buf, ' ');
          if (sp)
            *sp = '\0';
          wlrRandrOutput_ = buf;
        }
        pclose(fp);
      }
      if (!wlrRandrOutput_.empty()) {
        methods_.push_back(Method::WLR_RANDR);
        LOG_I("Display", "Detected screen control: wlr-randr ({})",
              wlrRandrOutput_);
      }
    }
  }

  // 8. VT Console blanking
  if (access("/dev/tty1", W_OK) == 0 || access("/dev/tty0", W_OK) == 0) {
    methods_.push_back(Method::CONSOLE);
    LOG_I("Display", "Detected screen control: Console VT blanking");
  }

  // 9. Always add SOFTWARE blanking as a robust fallback
  methods_.push_back(Method::SOFTWARE);
  LOG_I("Display", "Detected screen control: Software blanking");
#endif
}

void DisplayPower::setDrmFd(int fd) {
  drmFd_ = fd;
  LOG_I("Display", "DRM master fd set to {} (provided by SDL2 KMSDRM)", fd);
}

void DisplayPower::excludeMethod(Method m) {
  methods_.erase(std::remove(methods_.begin(), methods_.end(), m),
                 methods_.end());
  LOG_I("Display", "Excluded display power method: {}", methodToString(m));
}

bool DisplayPower::setPower(bool on) {
  bool success = false;

#if defined(__EMSCRIPTEN__) || defined(_WIN32)
  (void)on;
  success = false;
#else
  auto attempt = [&](Method m) -> bool {
    switch (m) {
    case Method::VCGENCMD:
      return runVcgencmd(on);
    case Method::BL_POWER:
      return writeSysfs(blPowerPath_, on ? "0" : "1");
    case Method::FRAMEBUFFER:
      return blankFramebuffer(!on);
    case Method::SOFTWARE:
      return true;
    case Method::XSET_DPMS:
      return runXsetDpms(on);
    case Method::TVSERVICE:
      return runTvservice(on);
    case Method::WLR_RANDR:
      return runWlrRandr(on);
    case Method::DRM_DPMS:
#ifdef HAS_LIBDRM
      return runDrmDpms(on);
#else
      return false;
#endif
    case Method::CONSOLE:
      return runConsoleBlank(on);
    case Method::NONE:
      return false;
    }
    return false;
  };

  if (selectedMethod_ != Method::NONE) {
    success = attempt(selectedMethod_);
  } else {
    for (auto method : methods_) {
      if (attempt(method)) {
        success = true;
        break;
      }
    }
  }
#endif

  if (success) {
    currentPower_ = on;
    if (!on)
      needsBlackFrame_ = true;
    SoundManager::getInstance().setScreenOn(on);
    LOG_I("Display", "Screen power set to {}", on ? "ON" : "OFF");
  } else {
#ifndef __EMSCRIPTEN__
    LOG_E("Display", "Failed to set screen power to {}", on ? "ON" : "OFF");
#endif
  }
  return success;
}

bool DisplayPower::getPower() const { return currentPower_; }

bool DisplayPower::consumeBlackFrame() {
  if (needsBlackFrame_) {
    needsBlackFrame_ = false;
    return true;
  }
  return false;
}

std::string DisplayPower::getMethodName() const {
#if defined(__EMSCRIPTEN__)
  return "none";
#else
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
    case Method::XSET_DPMS:
      name += "xset dpms";
      break;
    case Method::TVSERVICE:
      name += "tvservice";
      break;
    case Method::WLR_RANDR:
      name += "wlr-randr";
      break;
    case Method::DRM_DPMS:
      name += "drm dpms";
      break;
    case Method::CONSOLE:
      name += "console";
      break;
    case Method::NONE:
      name += "none";
      break;
    }
  }
  return name.empty() ? "none" : name;
#endif
}

std::string DisplayPower::findBacklightPowerPath() {
#if !defined(__EMSCRIPTEN__)
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
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
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
}

bool DisplayPower::runVcgencmd(bool on) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  std::string cmd =
      on ? "vcgencmd display_power 1" : "vcgencmd display_power 0";
  return std::system((cmd + " > /dev/null 2>&1").c_str()) == 0;
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::blankFramebuffer(bool blank) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  int fd = open("/dev/fb0", O_RDWR);
  if (fd < 0)
    return false;

  if (blank) {
    if (ioctl(fd, FBIOBLANK, FB_BLANK_POWERDOWN) < 0) {
      close(fd);
      return false;
    }
  } else {
    if (ioctl(fd, FBIOBLANK, FB_BLANK_UNBLANK) < 0) {
      close(fd);
      return false;
    }
  }

  close(fd);
  return true;
#else
  (void)blank;
  return false;
#endif
}

bool DisplayPower::binaryExists(const char *name) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
  std::string cmd = std::string("which ") + name + " > /dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
#else
  (void)name;
  return false;
#endif
}

bool DisplayPower::runXsetDpms(bool on) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
  std::string cmd;
  if (!xDisplayEnv_.empty())
    cmd = "DISPLAY=" + xDisplayEnv_ + " ";
  cmd += on ? "xset dpms force on > /dev/null 2>&1"
            : "xset dpms force off > /dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::runTvservice(bool on) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
  std::string cmd =
      on ? "tvservice -p > /dev/null 2>&1" : "tvservice -o > /dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::runWlrRandr(bool on) {
#if !defined(__EMSCRIPTEN__) && !defined(_WIN32)
  if (wlrRandrOutput_.empty())
    return false;
  std::string cmd;
  if (!wlDisplayEnv_.empty())
    cmd = "WAYLAND_DISPLAY=" + wlDisplayEnv_ + " ";
  cmd += "wlr-randr --output " + wlrRandrOutput_ + (on ? " --on" : " --off") +
         " > /dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::runDrmDpms(bool on) {
#if defined(HAS_LIBDRM) && !defined(__EMSCRIPTEN__)
  bool ownedFd = false;
  int fd = drmFd_;
  if (fd < 0) {
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
      LOG_E("DisplayPower", "Failed to open /dev/dri/card0: %s",
            std::strerror(errno));
      return false;
    }
    ownedFd = true;
  }

  drmModeRes *res = drmModeGetResources(fd);
  if (!res) {
    LOG_E("DisplayPower", "drmModeGetResources failed");
    if (ownedFd)
      close(fd);
    return false;
  }

  bool anySuccess = false;
  uint32_t dpmsValue = on ? 0 : 3; // 0=ON, 3=OFF

  for (int i = 0; i < res->count_connectors; i++) {
    drmModeConnector *conn = drmModeGetConnector(fd, res->connectors[i]);
    if (conn) {
      if (conn->connection == DRM_MODE_CONNECTED) {
        uint32_t propId = 0;
        for (int j = 0; j < conn->count_props; j++) {
          drmModePropertyRes *prop = drmModeGetProperty(fd, conn->props[j]);
          if (prop) {
            if (std::strcmp(prop->name, "DPMS") == 0) {
              propId = prop->prop_id;
            }
            drmModeFreeProperty(prop);
            if (propId != 0)
              break;
          }
        }

        if (propId) {
          if (drmModeConnectorSetProperty(fd, conn->connector_id, propId,
                                          dpmsValue) == 0) {
            anySuccess = true;
          } else {
            LOG_E("DisplayPower", "Failed to set DPMS on connector {}: {}",
                  conn->connector_id, std::strerror(errno));
          }
        }
      }
      drmModeFreeConnector(conn);
    }
  }

  drmModeFreeResources(res);
  if (ownedFd)
    close(fd);
  return anySuccess;
#else
  (void)on;
  return false;
#endif
}

bool DisplayPower::runConsoleBlank(bool on) {
#if defined(__linux__) && !defined(__EMSCRIPTEN__)
  // Attempt to blank via VT ioctl on /dev/tty1
  int fd = open("/dev/tty1", O_RDWR);
  if (fd < 0) {
    fd = open("/dev/tty0", O_RDWR);
  }
  if (fd < 0) {
    LOG_E("DisplayPower", "Failed to open /dev/tty1 or /dev/tty0 for blanking");
    return false;
  }

  // TIOCL_BLANKSCREEN/TIOCL_UNBLANKSCREEN
  char arg = on ? TIOCL_UNBLANKSCREEN : TIOCL_BLANKSCREEN;
  if (ioctl(fd, TIOCLINUX, &arg) < 0) {
    LOG_E("DisplayPower", "Console blanking ioctl failed: %s",
          std::strerror(errno));
    close(fd);
    return false;
  }

  close(fd);
  return true;
#else
  (void)on;
  return false;
#endif
}

std::string DisplayPower::methodToString(Method m) {
  switch (m) {
  case Method::VCGENCMD:
    return "vcgencmd";
  case Method::BL_POWER:
    return "sysfs bl_power";
  case Method::FRAMEBUFFER:
    return "framebuffer blank";
  case Method::SOFTWARE:
    return "software";
  case Method::XSET_DPMS:
    return "xset dpms";
  case Method::TVSERVICE:
    return "tvservice";
  case Method::WLR_RANDR:
    return "wlr-randr";
  case Method::DRM_DPMS:
    return "drm dpms";
  case Method::CONSOLE:
    return "console";
  case Method::NONE:
    return "auto";
  }
  return "auto";
}

DisplayPower::Method DisplayPower::stringToMethod(const std::string &s) {
  if (s == "vcgencmd")
    return Method::VCGENCMD;
  if (s == "sysfs bl_power")
    return Method::BL_POWER;
  if (s == "framebuffer blank")
    return Method::FRAMEBUFFER;
  if (s == "software")
    return Method::SOFTWARE;
  if (s == "xset dpms")
    return Method::XSET_DPMS;
  if (s == "tvservice")
    return Method::TVSERVICE;
  if (s == "wlr-randr")
    return Method::WLR_RANDR;
  if (s == "drm dpms")
    return Method::DRM_DPMS;
  if (s == "console")
    return Method::CONSOLE;
  return Method::NONE;
}
