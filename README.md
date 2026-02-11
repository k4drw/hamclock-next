# HamClock-Next

HamClock-Next is a modern, SDL2-based reconstruction of the classic HamClock. It focuses on visual fidelity, high-DPI support, and a smooth user experience while maintaining the essential functionality loved by amateur radio operators.

## 🚀 Key Accomplishments

### Visuals & Rendering
- **High-DPI Support**: Implemented text super-sampling and logical scaling, ensuring crisp text and graphics on 4K/Retina displays.
- **Visual Fidelity Mode**: A layout system that matches the original HamClock canonical pixel positions (800x480) with sub-pixel precision.
- **Graphics Smoothing**: Enabled 4x MSAA (Multi-Sample Anti-Aliasing) and bilinear filtering for smooth map lines and satellite tracks.
- **Advanced Vector Graphics**: Integrated hardware-accelerated ribbon paths and continuous polylines for seamless great circle and satellite tracks, along with differentiated, high-contrast markers for receivers and transmitters.

- **Dynamic UI**: Responsive resizing with automatic font re-rasterization to maintain quality.

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
- **CMake**: Version 3.20 or higher
- **C++20 Compiler**: e.g., GCC 15.2.1 or newer
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

## 🗺️ Roadmap & Next Steps

Based on the [PROJECT_STATUS.md](PROJECT_STATUS.md), the following features are planned:

- [ ] **Phase 13.3: DX Cluster Integration** — Real-time Telnet/UDP DX Cluster spot management and display.
- [ ] **Propogation Panel** — Band conditions and signal modeling.
- [ ] **Contest Widget** — Upcoming ham radio contest tracking.
- [ ] **On-The-Air Widget** — Monitoring POTA/SOTA/IOTA activity.
- [ ] **Enhanced Map Layers** — Weather, gray line optimizations, and more interactive spot filtering.

---
*Created with ❤️ by the HamClock-Next team.*
