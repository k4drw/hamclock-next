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

### 📡 Live Spots / PSK Reporter Shows Zero Spots

**Symptom**: The Live Spots widget shows all zeros and nothing appears on the map.

**Things to check:**

1. **No login is needed.** PSK Reporter has no credentials in HamClock-Next. It queries the open PSK Reporter API using the callsign set in Setup → **Station**. If you think you need to "sign in" somewhere, you don't — just make sure your callsign is set.

2. **Check the mode.** The default mode is *of DE* — "who heard me?" This shows stations that decoded *your* transmissions. If you haven't been transmitting recently, or no one decoded you during the time window, the count will be zero. Try switching to **by DE** mode (click **Counts** at the bottom of the widget to open settings) to see signals decoded in your area instead.

3. **Check your band toggles.** Click any band cell to make sure at least one band is colored (active). All-dark cells mean no bands are enabled for map plotting, though the counts will still show.

4. **PSK Reporter results are cached for ~10 minutes.** After changing settings, wait up to 10 minutes for the next fetch cycle before concluding spots are missing.

5. **Widen Max Age.** In the Counts overlay, try increasing Max Age to 60 or 90 minutes to capture a broader time window.

6. **Band activity.** Zero spots can simply mean no activity on those bands in that time window from your location. PSK Reporter only reports what was actually decoded — if the bands are quiet, the widget will be quiet too.

---

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
