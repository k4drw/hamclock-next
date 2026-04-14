# Troubleshooting & FAQ

This document covers common setup issues and runtime environment tips for HamClock-Next.

## 🚀 Raspberry Pi & Linux

### 🖼️ No Video / SDL_Init Failed
**Symptom**: `SDL_Init failed: No available video device`

**Solution**: 
1. If running without X11 (console mode), your user must be in the `video` and `render` groups:
   ```bash
   sudo usermod -aG video,render $USER
   ```
2. If using the KMSDRM driver on Raspberry Pi, ensure no other display server (like X11 or Wayland) is holding the DRM master lock.
3. For specialized headless or extreme-low-memory setups, try forcing software rendering:
   ```bash
   ./hamclock-next --software
   ```

### 🌓 Rendering Consistency
HamClock-Next includes specialized compatibility paths for artifact-free rendering on Raspberry Pi. If you see visual glitches, ensure you are running with the `kmsdrm` driver and have adequate GPU memory allocated in `/boot/config.txt`.

### 📦 Installation Errors (.deb)
**Symptom**: "cannot execute binary file: Exec format error"

**Solution**: This usually means you are trying to install a package that doesn't match your Operating System's architecture.
1. Check your architecture: `uname -m`
2. If it says `aarch64`, you need the **arm64** package.
3. If it says `armv7l`, you need the **armhf** package.
4. **Note**: Do not try to run the `.deb` file directly. Install it using `apt` or `dpkg`:
   ```bash
   sudo apt install ./hamclock-next_1.5.0_arm64.deb
   ```

---

## 💾 General

### 🔍 Waiting for Data
**Symptom**: Widgets show "Waiting for data".

**Solution**:
1. Check your internet connection.
2. Verify that your system time is accurate. SSL/TLS requires a correct system clock for certificate validation. Without valid time, API requests will fail.

### ⚙️ Resetting Configuration
**Symptom**: Corrupt settings or failed startup.

**Solution**: Delete the configuration file to start fresh:
- **Linux / Raspberry Pi**: `rm ~/.config/hamclock-next/config.json`
- **macOS**: `rm ~/Library/Application\ Support/hamclock-next/config.json`
- **Windows**: `del %APPDATA%\hamclock-next\config.json`

HamClock-Next will recreate the file and open the Setup screen on the next launch.

---

## 🪟 Windows

### Web server not accessible
Ensure Windows Firewall allows inbound connections on port 8080 (or your custom HTTP port).

### Build issues
Use `scripts/build-win64.sh` with Docker (dockcross). Do not attempt a native Windows build — cross-compile via Linux/Docker is the supported method.
