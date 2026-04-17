# Raspberry Pi Setup Guide

HamClock-Next runs well on the Raspberry Pi family (Pi 3, 4, 400, and 5), with or without a desktop.

## Installation

The easiest way to install HamClock-Next on a Raspberry Pi is to use the provided `.deb` package. Download it from the [Releases page](https://github.com/k4drw/hamclock-next/releases).

1. Determine your architecture:
   ```bash
   uname -m
   ```
   Use `arm64` for 64-bit Pi OS, or `armhf` for 32-bit Pi OS.
2. Install via `apt`:
   ```bash
   sudo apt install ./hamclock-next_*.deb
   ```

## Running Without a Desktop (Headless / Kiosk Mode)

HamClock-Next can run without X11 or Wayland. It can draw directly to the screen with [KMSDRM](Glossary.md#kmsdrm).

1. Add your user to the `video` and `render` groups:
   ```bash
   sudo usermod -aG video,render $USER
   ```
2. Log out and back in to apply the group changes.
3. Run with the [KMSDRM](Glossary.md#kmsdrm) driver:
   ```bash
   SDL_VIDEODRIVER=kmsdrm hamclock-next --fullscreen
   ```

### Autostart via systemd (FB0 Package)

If you installed the `FB0` (Framebuffer/Kiosk) debian package, a robust systemd service (`hamclock.service`) is already included and configured to run HamClock-Next headlessly on boot.

**You do not need to do anything else.** The package installation process automatically enables and starts the service for you.

*(Note: If you built from source instead of using the FB0 package, you will need to manually create a `/etc/systemd/system/hamclock.service` file that launches `/usr/bin/hamclock-next --fullscreen` with `Environment=SDL_VIDEODRIVER=kmsdrm` set).*

## Automatic Screen Brightness

HamClock-Next can control the screen brightness automatically. There are two ways to do this:

### Schedule-based dimming

Set dim and bright times (hour and minute) in Setup → **Appearance** → Brightness Schedule. HamClock-Next will reduce the brightness at the dim time and restore it at the bright time every day. You can also set an idle timeout — the screen blanks after a set number of minutes with no interaction.

See `brightnessSchedule`, `dimHour`, `brightHour`, and `idleMinutes` in [Setup & Configuration](Configuration.md).

### Light sensor auto-dimming (LTR329)

For automatic dimming without a fixed schedule, you can connect an **LTR329** ambient light sensor to your Pi's [I²C](Glossary.md#i2c) pins (GPIO 2 / SDA and GPIO 3 / SCL). HamClock-Next detects the sensor on startup and adjusts display brightness based on room lighting.

No software configuration is needed beyond wiring the sensor. If HamClock-Next finds the sensor, it takes over brightness control automatically.

> Note: The LTR329 is a small, inexpensive I²C sensor available from many electronics suppliers. It does not require any additional drivers on Raspberry Pi OS.

---

## Performance Tips

**Low Memory Boards (Pi 3 or older):**
* Ensure you allocate at least 128MB to the GPU if using KMSDRM rendering in `raspi-config` under `Performance Options`.
* If building from source, limit parallel compilation to avoid running out of memory: `cmake --build . -j1`.

**Software Rendering Fallback:**
If you cannot get hardware-accelerated graphics working, you can force the software renderer:
```bash
hamclock-next --software --fullscreen
```
