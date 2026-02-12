# HamClock-Next

HamClock-Next is a modern, SDL2-based reconstruction of the classic HamClock. It focuses on visual fidelity, high-DPI support, and a smooth user experience while maintaining the essential functionality loved by amateur radio operators.

## 🚀 Key Accomplishments

### Visuals & Rendering
- **High-Fidelity Rendering**: Implemented text super-sampling and logical scaling for crisp 4K/Retina display.
- **Dynamic Seasonal Maps**: Automatically downloads high-resolution NASA Blue Marble backgrounds based on the current month, with a vector grid fallback for offline use.
- **Zero-Asset Binary**: Essential assets (fonts, icons) are embedded directly into the executable, creating a self-contained single-file deployment.
- **Graphics Smoothing**: Hardware-accelerated anti-aliasing (4x MSAA) and ribbon-path primitives for smooth great circles and satellite tracks.
- **Advanced Caching**: Implemented a "lean" caching system using HTTP HEAD validation (`Last-Modified`/`ETag`) to save bandwidth by avoiding redundant downloads.

### Features
- **Global Map**: Interactive map with day/night terminator, great circle paths, and real-time overlays.
- **Live Spots**: Real-time band activity visualization using PSK Reporter data, with map plotting for selected bands.
- **Satellite Tracking**: Comprehensive satellite tracking with a high-fidelity polar plot, rise/set predictions, and map footprints.
- **Smart Setup**: Easy configuration of callsign and location via Maidenhead grid squares or direct map interaction (Shift-Click to set DE).
- **RSS News Banner**: Smoothly scrolling news ticker aggregating multiple amateur radio news feeds.
- **Solar Weather**: Integrated Space Weather data visualization.

## 🛠️ Requirements & Dependencies

To compile HamClock-Next, you will need the following libraries and tools installed on your system:

### System Libraries
- **SDL2**: Simple DirectMedia Layer 2.0 (core graphics and input)
- **SDL2_ttf**: SDL2 TrueType Font library
- **SDL2_image**: SDL2 Image loading library
- **libcurl**: Client URL library (for data fetching)
- **Threads**: POSIX threads (built-in on most Linux systems)

### Tools
- **CMake**: Version 3.18 or higher (compatible with RPi OS Bookworm)
- **C++20 Compiler**: e.g., GCC 12.2 (Pi OS 12) or newer
- **Make/Ninja**: Build automation tools

*Note: `nlohmann_json` and `libpredict` are automatically fetched and built via CMake during the first compilation.*

## 🏗️ Build Instructions

1. **Install dependencies** (Example for Ubuntu/Debian):
   ```bash
   sudo apt-get install cmake build-essential libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev
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

## 🛠️ Raspberry Pi & Console Mode (No X11)

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

## 🗺️ Roadmap & Next Steps

Based on the [PROJECT_STATUS.md](PROJECT_STATUS.md), the following features are planned:

- [ ] **Phase 13.3: DX Cluster Integration** — Real-time Telnet/UDP DX Cluster spot management and display.
- [ ] **Propogation Panel** — Band conditions and signal modeling.
- [ ] **Contest Widget** — Upcoming ham radio contest tracking.
- [ ] **On-The-Air Widget** — Monitoring POTA/SOTA/IOTA activity.
- [ ] **Enhanced Map Layers** — Weather, gray line optimizations, and more interactive spot filtering.

---
*Created with ❤️ by the HamClock-Next team.*
