# Troubleshooting HamClock-Next

This document covers common setup issues and runtime environment tips.

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

---

## 💾 General

### 🔍 Waiting for Data
**Symptom**: Widgets show "Waiting for data".
**Solution**:
1. Check your internet connection.
2. Verify that your system time is accurate. SSL/TLS requires a correct system clock for certificate validation.

### ⚙️ Resetting Configuration
**Symptom**: Corrupt settings or failed startup.
**Solution**: Delete the configuration directory to start fresh:
- **Linux**: `rm -rf ~/.local/share/HamClock/HamClock-Next/`
- **macOS**: `rm -rf ~/Library/Application\ Support/HamClock/HamClock-Next/`
- **Windows**: `rmdir /s /q %APPDATA%\HamClock\HamClock-Next\`

---

## 🪟 Windows

### Web server not accessible
Ensure Windows Firewall allows inbound connections on port 8080.

### Build issues
Use `scripts/build-win64.sh` with Docker (dockcross). Do not attempt a native Windows build — cross-compile via Linux/Docker is the supported method.

### Installer
`build-win64/HamClock-Next-Setup.exe` is an NSIS installer that sets up PATH and creates Start Menu shortcuts.
