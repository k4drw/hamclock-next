# Raspberry Pi Setup Guide

HamClock-Next is fully optimized to run on the Raspberry Pi family (Pi 3, 4, 400, and 5) natively or without an X11 windowing environment.

## Installation

The easiest way to install HamClock-Next on a Raspberry Pi is using the provided `.deb` package. Download it from the [Releases page](https://github.com/k4drw/hamclock-next/releases).

1. Determine your architecture (`uname -m`). Use `arm64` for 64-bit Pi OS, or `armhf` for 32-bit Pi OS.
2. Install via `apt`:
   ```bash
   sudo apt install ./hamclock-next_*.deb
   ```

## Running Without a Desktop (Headless / Kiosk Mode)

HamClock-Next uses SDL2, meaning it doesn't require X11 or Wayland. It can write directly to the screen via the Linux Direct Rendering Manager (DRM).

1. Add your user to the `video` and `render` groups:
   ```bash
   sudo usermod -aG video,render $USER
   ```
2. Log out and back in to apply the group changes.
3. Run with the KMSDRM driver:
   ```bash
   SDL_VIDEODRIVER=kmsdrm hamclock-next --fullscreen
   ```

### Autostart via systemd (FB0 Package)

If you installed the `FB0` (Framebuffer/Kiosk) debian package, a robust systemd service (`hamclock.service`) is already included and configured to run HamClock-Next headlessly on boot.

**You do not need to do anything else.** The package installation process automatically enables and starts the service for you.

*(Note: If you built from source instead of using the FB0 package, you will need to manually create a `/etc/systemd/system/hamclock.service` file that launches `/usr/bin/hamclock-next --fullscreen` with `Environment=SDL_VIDEODRIVER=kmsdrm` set).*

## Performance Tips

**Low Memory Boards (Pi 3 or older):**
* Ensure you allocate at least 128MB to the GPU if using KMSDRM rendering in `raspi-config` under `Performance Options`.
* If building from source, limit parallel compilation to avoid running out of memory: `cmake --build . -j1`.

**Software Rendering Fallback:**
If you cannot get hardware-accelerated graphics working, you can force the software renderer:
```bash
hamclock-next --software --fullscreen
```
