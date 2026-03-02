# Getting Started

This page covers building HamClock-Next from source, running it for the first time, and completing initial setup.

---

## Prerequisites

| Platform | Requirements |
|----------|-------------|
| Linux (native) | CMake ≥ 3.16, SDL2, SDL2_ttf, SDL2_image, libcurl, gcc/clang |
| macOS | Same as Linux; use Homebrew for SDL2 packages |
| Windows (cross-build) | Docker + dockcross/windows-static-x64 |
| Browser (WASM) | Docker + Emscripten image (via build script) |

---

## Building

### Native (Linux / macOS)

```bash
git clone https://github.com/your-org/hamclock-next.git
cd hamclock-next
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target hamclock-next
```

The binary is produced at `build/hamclock-next`.

### Browser (WebAssembly)

```bash
./scripts/build-wasm.sh
# or, using Docker:
./scripts/build-wasm-docker.sh
```

Serve the output directory with any HTTP server, then open `live.html`.

### Windows (x64)

```bash
./scripts/build-win64.sh
```

Produces `build-win64/hamclock-next.exe` and `build-win64/HamClock-Next-Setup.exe`.

---

## First Launch

### Native

```bash
./build/hamclock-next
```

### Web (live view)

```bash
./build/hamclock-next --live-web
# Then open: http://localhost:8080/live.html
```

---

## Initial Setup

On first launch, HamClock-Next opens the **Setup screen**.

![Setup modal](images/modal-setup.png)

Fill in these fields:

| Field | Description |
|-------|-------------|
| **Callsign** | Your amateur radio callsign (e.g., `W1AW`) |
| **Grid** | Your Maidenhead grid locator (e.g., `FN31`) |
| **Latitude** | Your latitude in decimal degrees (auto-filled from grid if left blank) |
| **Longitude** | Your longitude in decimal degrees (auto-filled from grid if left blank) |

After confirming, HamClock-Next starts fetching data and displays the main dashboard.

You can return to setup at any time by clicking the **gear icon** (⚙) in the Time Panel at the top-left.

---

## First-Run Tour

Once the dashboard is running:

1. **Time Panel** (top-left) — shows UTC and local time, DE callsign/grid, sunrise/sunset. Click the gear icon to reopen Setup.
2. **Six panes** — the dashboard is divided into six rotatable panes, each cycling through one or more widgets.
3. **Map** — the large center area shows the world map with configurable overlays.
4. **Click the top strip of any pane** — opens the widget picker so you can choose what that pane displays.
5. **Press K** — highlights every interactive region on screen with cyan boxes and tooltips.

See [Screen Layout](Layout) for a detailed annotated diagram.
