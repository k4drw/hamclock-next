# Building from Source

> **Most users should install a release package instead.**
> Download a pre-built binary for your platform from [Getting Started](Getting-Started.md).
> Building from source is for developers, packagers, and custom configurations.

---

## Prerequisites

| Platform | Requirements |
|----------|-------------|
| Linux (native) | CMake ≥ 3.18, SDL2, SDL2_ttf, SDL2_image, libcurl, gcc/clang |
| macOS | Same as Linux; use Homebrew for SDL2 packages |
| Windows (cross-build) | Docker + dockcross/windows-static-x64 |
| Browser (WASM) | Docker + Emscripten image (via build script) |

### Installing Dependencies

**Debian / Ubuntu:**
```bash
sudo apt install cmake libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libcurl4-openssl-dev
```

**Fedora / RHEL:**
```bash
sudo dnf install cmake SDL2-devel SDL2_ttf-devel SDL2_image-devel libcurl-devel
```

**OpenSUSE:**
```bash
sudo zypper install cmake libSDL2-devel libSDL2_ttf-devel libSDL2_image-devel libcurl-devel
```

**macOS (Homebrew):**
```bash
brew install cmake sdl2 sdl2_ttf sdl2_image curl
```

---

## Building

### Native (Linux / macOS)

```bash
git clone https://github.com/your-org/hamclock-next.git
cd hamclock-next
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target hamclock-next -j$(nproc)
```

The binary is produced at `build/hamclock-next`.

### Browser (WebAssembly)

```bash
./scripts/build-wasm.sh
# or, using Docker:
./scripts/build-wasm-docker.sh
```

Serve the output directory with any HTTP server, then open `live.html`.

### Windows (x64, cross-compile via Docker)

```bash
./scripts/build-win64.sh
```

Produces `build-win64/hamclock-next.exe` and `build-win64/HamClock-Next-Setup.exe`.

---

## Running After Building

### Native

```bash
./build/hamclock-next
```

**On a Raspberry Pi or console system without a desktop environment:**
```bash
SDL_VIDEODRIVER=kmsdrm ./build/hamclock-next --fullscreen
```

### Web / Live View

```bash
./build/hamclock-next --live-web
# Then open: http://localhost:8080/live.html
```

### Command-Line Options

| Option | Description |
|--------|-------------|
| `-f`, `--fullscreen` | Launch in fullscreen mode |
| `-s`, `--software` | Force software renderer (no GPU) |
| `--live-web` | Enable the live web control interface |
| `--no-audio` | Disable audio and TTS |
| `--log-level DEBUG` | Verbose logging |
| `-h`, `--help` | Show all options |
