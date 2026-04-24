# HamClock-Next (v1.6)

> **HamClock-Next** is a modern, multi-platform rewrite of the original HamClock
> by **Elwood Downey, WB0OEW** (SK 29 January 2026).
>
> Elwood built HamClock into one of the most beloved shack displays in amateur
> radio — a real-time world clock, propagation tool, and DX dashboard all in one.
> He maintained it for years with care and craftsmanship, and the ham community
> is lesser for his passing. HamClock-Next carries his work forward with deep
> respect, preserving every feature of the original while adding a modern
> architecture and new capabilities for today's ham shacks.
>
> *This project is dedicated to his memory. 73, Elwood.*

---

## Why a Complete Rewrite?

HamClock-Next did not start as a rewrite. It started as a refactor — and a
frustrating one.

Elwood originally wrote HamClock for the **ESP8266** microcontroller. Over time
he shimmed in Linux support around that embedded core. By version 4 he had
formally dropped ESP8266 support, but the architectural fingerprint of
single-file globals, Arduino-style execution flow, and tight coupling to his own
propagation proxy remained woven throughout the codebase. It was never the kind
of code you could fault — it was *brilliant* embedded engineering — but it was
also not a foundation you could cleanly modernize.

The decision to start clean was not made lightly, but it was the right call.
A greenfield CMake + SDL2 + modern C++20 project with a proper layer separation
(data stores, providers, UI widgets, network) turned out to move faster than
the refactor had — and the result is something that can actually be maintained,
extended, and ported without archaeology.

---

### Project Philosophy — AI as Co-Developer

HamClock-Next is, by deliberate design, a near-**100% AI-written codebase**.
The implementation itself is written by AI assistants while humans serve as architects: 
defining the structure, owning the design decisions, and reviewing output.

#### Multi-Agent Consensus & Model Diversity
A cornerstone of our methodology is the use of **diverse AI models** (Gemini 3, Claude Code, and Codex) to perform peer-reviewed implementation and code audits. By leveraging the unique training biases of different agents, we achieve a "multi-agent consensus" that eliminates hallucinations and surfaces architectural edge cases, resulting in a more robust and stable codebase than any single model could produce alone.


The result is a production-quality, modern C++ codebase that compiles clean, 
passes valgrind, and runs stably on everything from a Raspberry Pi 3 to a browser tab.

---

## 100% Feature Parity with the Original HamClock

HamClock-Next implements all **82 original features** across every category:

| Category                | Features                                                                                                                                 |
| ----------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| World Map & Projections | Azimuthal, Robinson, Mercator; day/night terminator; great-circle paths; Maidenhead/lat-lon grid overlays                                |
| Map Markers             | City labels, DX Cluster spots, PSK Reporter spots, POTA/SOTA activator pins, ADIF QSO pins, satellite tracks                             |
| Map Overlays            | VOACAP MUF, VOACAP Reliability, VOACAP TOA, KC2G real-time MUF, cloud cover, WX precipitation                                            |
| Data Panels             | Solar/space weather, DX Cluster, Live Spots (PSK/WSPR/RBN), POTA/SOTA activity, satellite tracking, contest calendar, RSS feed, and more |
| Core UI                 | Clock display, callsign, DE/DX info panels, bearing/distance, countdown timer, audio alarms                                              |
| Hardware                | GPS/NMEA (gpsd), rotator control (rotctld), rig CAT control (rigctld), display brightness, display power management                      |
| Utilities               | QRZ.com lookup, Maidenhead grid system, callsign prefix lookup, EME (moonbounce) tool, Santa Tracker                                     |

---

## 🆕 New Features Exclusive to HamClock-Next

### 1. Multi-Display Hub Model
One HamClock-Next instance runs as a **Master hub** and fetches all external data. All other instances on the same LAN run as **Clients** and pull data from the hub. Eliminates redundant API calls and ensures data consistency across every screen in a club station.

### 2. Smart Network Caching
Every external data fetch is cached both in memory and on disk with ETag / HTTP 304 support. Dramatically reduces bandwidth on slow or metered connections. Cache survives restarts — data is available immediately on next launch.

### 3. Live Web Viewer & Remote Control
- MJPEG live stream of the HamClock screen, viewable in any browser.
- Full **mouse and keyboard passthrough** — click and type through the browser.
- REST API with **79 endpoints** for automation and home-automation integration.

### 4. Advanced Weather & Environment
- **7-day NWS forecast panel** — hourly and daily outlook for your grid square.
- **NOAA severe weather alerts** — active watches, warnings, and advisories for your county.
- **Hurricane / tropical cyclone tracker** — active storm list with track and intensity.
- **Marine / tide panel** — NOAA tide predictions for nearby stations.

### 5. Ham Radio Hardware Integration
- **Hamlib rig control** — reads frequency and mode from your transceiver via `rigctld`.
- **Hamlib rotator control** — displays azimuth/elevation; accepts bearing commands from the map.
- **WSJT-X / JS8Call UDP** — captures QSO data live from digital mode software.

### 6. Expanded Radio Activity & Logging
- **ADIF log import** — plots confirmed QSOs as pins on the world map.
- **DX watchlist** — monitor specific callsigns/prefixes with on-screen alerts.
- **ONTA (On The Air) panel** — aggregated POTA/SOTA activations.

### 7. Space & Celestial
- **Asteroid Tracker** — next 5 close approaches from JPL SSD/CAD data.
- **History Panel** — scrollable time-series graphs of solar flux (SFI), SSN, and Kp.
- **Native VOACAP propagation engine** — MUF, reliability, and takeoff-angle heatmaps computed locally.

### 8. Modern UI & Customization
- **Dynamic Theme Engine** — Built-in color themes (Default, Dark, Glass) plus a powerful **Custom Theme** editor.
- **Real-time UI Preview** — Changes in the Theme Customizer are reflected immediately.
- **High-DPI Support** — Text super-sampling and logical scaling for crisp 4K/Retina displays.

---

## Download & Install

**Pre-built packages are available for all platforms — no compiler required.**

| Platform | Download |
|----------|---------|
| Linux (deb/rpm) | [GitHub Releases](https://github.com/k4drw/hamclock-next/releases) · [SourceForge mirror](https://sourceforge.net/projects/hamclock-next/files/) |
| macOS (.dmg) | [GitHub Releases](https://github.com/k4drw/hamclock-next/releases) |
| Windows (installer) | [GitHub Releases](https://github.com/k4drw/hamclock-next/releases) |
| Browser / WASM | Self-host via `--live-web` flag, or use the downloadable WASM build |

```bash
# Debian / Ubuntu
sudo dpkg -i hamclock-next_*.deb && hamclock-next

# OpenSUSE / Fedora
sudo rpm -i hamclock-next-*.rpm && hamclock-next

```

---

## Building from Source

> **Only needed if you want a custom build or are contributing to development.**
> See the [Building from Source](docs/wiki/Building-from-Source.md) wiki page for full instructions.

### System Libraries Required

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

---

### Build Steps

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
```

### Building on macOS

```bash
# Install dependencies via Homebrew
brew install cmake sdl2 sdl2_image sdl2_ttf curl

# Build .app bundle
./scripts/build-macos.sh
```

### Building for Windows (x64 Cross-Compile)

Requires Docker (image is auto-pulled on first run).

```bash
bash scripts/build-win64.sh
```

Produces `build-win64/hamclock-next.exe` and `build-win64/HamClock-Next-Setup.exe`.

---

## Raspberry Pi & Console Mode (No X11)

If you encounter `SDL_Init failed: No available video device`, ensure your user is in the `video` and `render` groups:
```bash
sudo usermod -aG video,render $USER
```
Then try:
```bash
# SDL_VIDEODRIVER=kmsdrm is the standard for RPi console mode
sudo SDL_VIDEODRIVER=kmsdrm ./hamclock-next --fullscreen --software
```

### Command Line Options
- `-f, --fullscreen`: Launch in fullscreen mode.
- `-s, --software`: Force software rendering (disables OpenGL/MSAA). Essential for environments without a functioning 3D setup or DRI access.
- `--log-level <level>`: Set logging verbosity. Values: `debug`, `info`, `warn`, `error` (default: `warn`).
- `-h, --help`: Show help message.

---

## Data & Configuration Locations

- **Linux / Raspberry Pi**: `~/.local/share/HamClock/HamClock-Next/`
- **Windows**: `%APPDATA%\HamClock\HamClock-Next\`
- **macOS**: `~/Library/Application Support/HamClock/HamClock-Next/`

---

## Contributing & AI Assistance (MCP)

HamClock-Next is designed for AI-assisted development using the **Model Context Protocol (MCP)**. We provide a specialized "HamClock Bridge" server that allows AI assistants (like Claude and Gemini) to compare codebases, trace logic, and automatically scaffold new widgets.

To get started with AI-assisted contributions, see the **[MCP_GUIDE.md](MCP_GUIDE.md)** for setup and usage instructions.

---

HamClock-Next is free and open source under the GPL license, in the spirit of the original work by Elwood Downey, WB0OEW.

*73 de K4DRW*
