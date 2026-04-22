# Troubleshooting & FAQ

This page covers the most common setup and runtime problems.

## Raspberry Pi & Linux

### No video
**Symptom**: `SDL_Init failed: No available video device`

**Try this first**:
1. If you are running without a desktop, make sure your user is in the `video` and `render` groups:
   ```bash
   sudo usermod -aG video,render $USER
   ```
2. Make sure nothing else is already using the display, such as X11 or Wayland.
3. If the machine is low on memory, try software rendering:
   ```bash
   ./hamclock-next --software
   ```

### Display looks wrong
If you see visual glitches on a Raspberry Pi, make sure you are using the `kmsdrm` driver and have enough GPU memory allocated in `/boot/config.txt`.

### Package will not install
**Symptom**: `Exec format error`

This usually means the package does not match your CPU type.
1. Check your architecture:
   ```bash
   uname -m
   ```
2. `aarch64` means you need `arm64`.
3. `armv7l` means you need `armhf`.
4. Install the package with `apt` or `dpkg`, not by running the `.deb` file directly.

## General

### Waiting for data
**Symptom**: Widgets show `Waiting for data`.

1. Check your internet connection.
2. Make sure your system clock is correct. Wrong time can break secure connections.

### Live Spots shows zero spots
**Symptom**: The Live Spots widget shows no activity.

1. No login is needed for PSK Reporter. Just make sure your callsign is set in Setup.
2. Check the mode. `of DE` means “who heard me?” If you have not been on the air recently, this can be zero. Try `by DE` to see stations heard near you.
3. Make sure at least one band is turned on in the widget.
4. Wait a few minutes after changing settings. Results are cached.
5. Increase `Max Age` to widen the search window.
6. Sometimes zero spots simply means there is no activity right now.

### Reset configuration
If settings become corrupted or the app will not start, delete the configuration file and launch again:
- **Linux / Raspberry Pi**: `rm ~/.config/hamclock-next/config.json`
- **macOS**: `rm ~/Library/Application Support/hamclock-next/config.json`
- **Windows**: `del %APPDATA%\hamclock-next\config.json`

HamClock-Next will recreate the file and open Setup on the next launch.

## Windows

### Web server not accessible
Make sure Windows Firewall allows inbound connections on port 8080, or on your custom HTTP port.

### Build issues
Use `scripts/build-win64.sh` with Docker. Native Windows builds are not supported.
