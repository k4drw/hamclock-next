# HamClock-Next (v0.8B)

HamClock-Next is a modern, SDL2-based reconstruction of the classic HamClock. It focuses on visual fidelity, high-DPI support, and a smooth user experience while maintaining the essential functionality loved by amateur radio operators.

> [!WARNING]
> **BETA RELEASE NOTICE**: This is a Beta release (**v0.8B**). While functional, it is still under active development and contains known bugs. Breaking changes and refinements are ongoing. Please report issues on GitHub.

## Key Accomplishments

### Visuals & Rendering
- **High-Fidelity Rendering**: Implemented text super-sampling and logical scaling for crisp 4K/Retina display. Includes specialized compatibility paths for artifact-free rendering on Raspberry Pi (KMSDRM).
- **Dynamic Seasonal Maps**: Automatically downloads high-resolution NASA Blue Marble backgrounds based on the current month.
- **Night Lights & Shadows**: Integrated NASA Black Marble city lights with additive blending and persistent vertex-shaded terminator overlays.
- **NASA Dial-a-Moon**: Real-time lunar phase imagery directly from NASA SVS, corrected for North-Up orientation.
- **Zero-Asset Binary**: Essential assets (fonts, icons) are embedded directly into the executable, creating a self-contained single-file deployment.
- **Graphics Smoothing**: Hardware-accelerated anti-aliasing (4x MSAA) and ribbon-path primitives for smooth great circles and satellite tracks.
- **Advanced Caching**: Implemented a "lean" caching system using HTTP HEAD validation (`Last-Modified`/`ETag`) to save bandwidth.

### Features
- **Global Map**: Interactive map with day/night terminator, great circle paths, and real-time overlays.
- **Live Spots**: Real-time band activity visualization using PSK Reporter data, with map plotting for selected bands.
- **DX Cluster**: Integrated Telnet-based DX Cluster management with interactive spot lists, age tracking, and prefix-resolved map plotting.
- **On-The-Air Widget**: Real-time monitoring of POTA, SOTA, and DXPedition activity with seamless JSON/HTML aggregation.
- **Satellite Tracking**: Comprehensive satellite tracking with a high-fidelity polar plot, rise/set predictions, and map footprints.
- **Space Weather**: Integrated Space Weather data visualization including:
  - **Aurora Graph & Map**: 24-hour history graph and real-time forecast overlays.
  - **DRAP Panel**: D-Region Absorption Prediction with color-coded severity indicators.
  - **Propagation Model**: Solar flux, sunspot numbers, and band-specific condition estimates.
- **Smart Setup**: Easy configuration of callsign and location via Maidenhead grid squares or direct map interaction (Shift-Click to set DE).
- **RSS News Banner**: Smoothly scrolling news ticker aggregating multiple amateur radio news feeds.

For a detailed guide on how to navigate the new interface and use keyboard shortcuts, see **[USAGE.md](USAGE.md)**.

## Requirements & Dependencies

To compile HamClock-Next, you will need the following libraries and tools installed on your system:

### System Libraries
- **SDL2**: Simple DirectMedia Layer 2.0 (core graphics and input)
- **SDL2_ttf**: SDL2 TrueType Font library
- **SDL2_image**: SDL2 Image loading library
- **libcurl**: Client URL library (for data fetching)
- **SQLite3**: Embedded database for persistent storage
- **Threads**: POSIX threads (built-in on most Linux systems)

### Tools
- **CMake**: Version 3.18 or higher (compatible with RPi OS Bookworm)
- **C++20 Compiler**: e.g., GCC 12.2 (Pi OS 12) or newer
- **Make/Ninja**: Build automation tools
- **pkg-config**: Package configuration tool (for finding SDL2 extensions)

*Note: `nlohmann_json`, `libpredict`, `cpp-httplib`, and `spdlog` are automatically fetched and built via CMake during the first compilation.*

## Build Instructions

1. **Install dependencies** (Example for Ubuntu/Debian):
   ```bash
   sudo apt-get install cmake build-essential pkg-config \
     libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
     libcurl4-openssl-dev libsqlite3-dev
   ```

2. **Configure and Build**:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

3. **Run**:
   ```bash
   ./hamclock-next
   ```
   *Optional: Use `-f` or `--fullscreen` to force start in fullscreen mode.*

### Building on Raspberry Pi

HamClock-Next builds natively on all Raspberry Pi models. For best results on memory-constrained devices:

```bash
# Install dependencies (Raspberry Pi OS / Raspbian)
sudo apt-get update
sudo apt-get install cmake build-essential pkg-config \
  libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev \
  libcurl4-openssl-dev libsqlite3-dev

# Clone the repository
git clone https://github.com/k4drw/hamclock-next.git
cd hamclock-next

# Build with single thread (safe for 1GB RAM)
mkdir build
cd build
cmake ..
cmake --build . -j1

# For automated universal builds (recommended for releases):
./scripts/build-linux-arm64-universal.sh   # Builds ARM64 DEBs (Unified + FB0)
./scripts/build-linux-armhf-universal.sh   # Builds ARMhf DEBs (Unified + FB0)
```

**Build times:**
- **Pi 3B (1GB)**: ~20-30 minutes (first build)
- **Pi 4 (4GB)**: ~10-15 minutes
- **Pi 5 (8GB)**: ~5-8 minutes

### Building on macOS

HamClock-Next builds natively on macOS (Intel and Apple Silicon):

```bash
# Install dependencies via Homebrew
brew install cmake sdl2 sdl2_image sdl2_ttf curl

# Clone the repository
git clone https://github.com/k4drw/hamclock-next.git
cd hamclock-next

# Build .app bundle
./scripts/build-macos.sh

# The application bundle will be at:
#   build-macos/hamclock-next.app
#
# Distribution packages:
#   build-macos/hamclock-next-macos-arm64.dmg  (or x86_64)
#   build-macos/hamclock-next-macos-arm64.zip

# To install: Open the DMG and drag HamClock-Next.app to Applications
# Or: Unzip and copy hamclock-next.app to /Applications
```

**Build times:**
- **M1/M2/M3 Mac**: ~2-5 minutes (first build)
- **Intel Mac**: ~5-10 minutes

### Building for Windows (x64 Cross-Compile)

Requires Docker (image is auto-pulled on first run).

```bash
bash scripts/build-win64.sh
```

Produces `build-win64/hamclock-next.exe` and `build-win64/HamClock-Next-Setup.exe`.

The installer (`HamClock-Next-Setup.exe`) sets up PATH and creates Start Menu shortcuts.

**Speed up rebuilds with ccache:**
```bash
sudo apt-get install ccache
cmake -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ..
cmake --build . -j1
```
With ccache, incremental builds take only **30-60 seconds**!

### Universal Linux Packages (Debian/Ubuntu/PiOS)

We provide high-compatibility **Universal Binaries** built against a Debian Bullseye baseline (glibc 2.31). These are distributed in two distinct `.deb` variants:

1. **`unified`**: The standard desktop package. Includes dependencies for X11, Wayland, and KMSDRM. Recommended for most users.
2. **`fb0`**: A lean "kiosk" package. Only includes dependencies for direct framebuffer (KMSDRM) rendering. Ideal for headless Raspberry Pi Lite installs.

For detailed technical references and remote control info, see:
- **[API.md](API.md)** - Remote control and debugging API reference
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Solutions for common build and runtime issues
- **[DATA_SOURCES.md](original-analysis/DATA_SOURCES.md)** - List of live data providers
- **[USAGE.md](USAGE.md)** - Interface navigation and keyboard shortcuts

**Note**: The web server live view is optimized for headless operation (1 FPS refresh) to reduce CPU usage (~50-60% vs ~100%). See **[API.md](API.md)** for customization options.


## Raspberry Pi & Console Mode (No X11)

If you are running on a headless or console-only system (like Pi-Star or Raspbian Lite) without X11/Wayland, you may need to use the software renderer.

### Running on Framebuffer (/dev/fb0)
If you encounter `SDL_Init failed: No available video device`, ensure your user is in the `video` and `render` groups:
```bash
sudo usermod -aG video,render $USER
```
Then try:
```bash
# SDL_VIDEODRIVER=kmsdrm is the standard for RPi console mode
sudo SDL_VIDEODRIVER=kmsdrm ./hamclock-next --fullscreen --software
```

### Preventing Screen Blanking
In console mode (no X11), the Linux kernel defaults to blanking the screen after 10 minutes of inactivity. To prevent this for fixed-display kiosks:

1. Edit `/boot/cmdline.txt` (or `/boot/firmware/cmdline.txt` on Bookworm):
   ```bash
   sudo nano /boot/cmdline.txt
   ```
2. Append `consoleblank=0` to the end of the line (all parameters must be on a single line).
3. Save and reboot: `sudo reboot`

HamClock-Next also attempts to disable this at runtime, but the kernel parameter is the most reliable method.

### Command Line Options
- `-f, --fullscreen`: Launch in fullscreen mode.
- `-s, --software`: Force software rendering (disables OpenGL/MSAA). Essential for environments without a functioning 3D setup or DRI access.
- `--log-level <level>`: Set logging verbosity. Values: `debug`, `info`, `warn`, `error` (default: `warn`).
- `-h, --help`: Show help message.

## Data & Configuration Locations

HamClock-Next stores configuration (`config.json`), database (`hamclock.db`), and cached assets (maps, satellite data) in standard platform-specific locations:

- **Linux / Raspberry Pi**: `~/.local/share/HamClock/HamClock-Next/`
- **Windows**: `%APPDATA%\HamClock\HamClock-Next\`
- **macOS (Apple Silicon Only)**: `~/Library/Application Support/HamClock/HamClock-Next/`

*Note: If you are upgrading from an older version, your previous configuration files at `~/.config/hamclock/` may need to be moved manually.*

## Contributing & AI Assistance (MCP)

HamClock-Next is designed for AI-assisted development using the **Model Context Protocol (MCP)**. We provide a specialized "HamClock Bridge" server that allows AI assistants (like Claude and Gemini) to:

-   **Compare Codebases**: Check feature parity between this project and the original HamClock.
-   **Trace Logic**: Find specific pointers to original implementation files.
-   **Automated Scaffolding**: Automatically generate C++ boilerplate for new panels and providers.
-   **Plan Work**: Generate implementation tickets with pre-analyzed technical context.

To get started with AI-assisted contributions, see the **[MCP_GUIDE.md](MCP_GUIDE.md)** for setup and usage instructions.

## Roadmap & Next Steps

- ✅ **Phase 6: Hardware Control** (RigService CAT, RotatorService, CPUMonitor) — DONE
- ✅ **Windows x64 cross-build** (dockcross) — DONE (02-17-2026)
- ✅ **GPS/NMEA**: GPSProvider integrated; time/location sync supported. — DONE (02-17-2026)
- [ ] **World Map Overlays** — CQ/ITU Zones, Prefix overlays
- [ ] **Multi-Instance Sync** — Sharing state between multiple clocks on the same network

---
*Created by the HamClock-Next team.*
