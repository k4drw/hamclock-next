# HamClock-Next (v0.5B)

HamClock-Next is a modern, SDL2-based reconstruction of the classic HamClock. It focuses on visual fidelity, high-DPI support, and a smooth user experience while maintaining the essential functionality loved by amateur radio operators.

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
git clone https://github.com/USER/hamclock-next.git
cd hamclock-next

# Build with single thread (safe for 1GB RAM)
mkdir build
cd build
cmake ..
cmake --build . -j1
```

**Build times:**
- **Pi 3B (1GB)**: ~20-30 minutes (first build)
- **Pi 4 (4GB)**: ~10-15 minutes
- **Pi 5 (8GB)**: ~5-8 minutes

**Speed up rebuilds with ccache:**
```bash
sudo apt-get install ccache
cmake -DCMAKE_C_COMPILER_LAUNCHER=ccache \
      -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
      ..
cmake --build . -j1
```
With ccache, incremental builds take only **30-60 seconds**!

For detailed optimization strategies and troubleshooting, see:
- **[RPI_BUILD_SUCCESS.md](RPI_BUILD_SUCCESS.md)** - Raspberry Pi build guide
- **[BUILD_OPTIMIZATION.md](BUILD_OPTIMIZATION.md)** - Memory optimization strategies
- **[SDL2_COMPATIBILITY.md](SDL2_COMPATIBILITY.md)** - SDL2 version compatibility
- **[DEBUG_API.md](DEBUG_API.md)** - Web server performance optimization

**Note**: The web server live view is optimized for headless operation (1 FPS refresh) to reduce CPU usage (~50-60% vs ~100%). See DEBUG_API.md for customization options.


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

*Note: On Raspberry Pi 3B, ensure `dtoverlay=vc4-kms-v3d` is NOT inside a `[pi4]` block in `/boot/config.txt`. It must be in the `[all]` section or at the top level to be active.*

### Command Line Options
- `-f, --fullscreen`: Launch in fullscreen mode.
- `-s, --software`: Force software rendering (disables OpenGL/MSAA). Essential for environments without a functioning 3D setup or DRI access.
- `-h, --help`: Show help message.

## Data & Configuration Locations

HamClock-Next stores configuration (`config.json`), database (`hamclock.db`), and cached assets (maps, satellite data) in standard platform-specific locations:

- **Linux / Raspberry Pi**: `~/.local/share/HamClock/HamClock-Next/`
- **Windows**: `%APPDATA%\HamClock\HamClock-Next\`
- **macOS**: `~/Library/Application Support/HamClock/HamClock-Next/`

*Note: If you are upgrading from an older version, your previous configuration files at `~/.config/hamclock/` may need to be moved manually.*

## Roadmap & Next Steps

Based on the [PROJECT_STATUS.md](PROJECT_STATUS.md), the following features are planned:

- [ ] **Phase 6: Hardware Control** — Rotator control (Hamlib/Rigctl) and BME280 environment sensor integration.
- [ ] **World Map Overlays** — Gray line optimizations, CQ/ITU Zones, and Prefix overlays.
- [ ] **Multi-Instance Sync** — Sharing state between multiple clocks on the same network.

---
*Created by the HamClock-Next team.*
