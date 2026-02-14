#!/bin/bash
# MacOS Build Script
# This script is intended to be run on macOS (either locally or in CI)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

# Check if running on macOS
if [[ "$(uname)" != "Darwin" ]]; then
    echo "ERROR: This script must be run on macOS."
    echo "       Building for macOS on Linux requires a complex cross-compilation toolchain."
    echo "       Please use the GitHub Actions workflow to generate macOS binaries."
    exit 1
fi

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Please install CMake (e.g., brew install cmake) to continue."
    exit 1
fi

echo "Using CMake to fetch and build dependencies locally (SDL2, Curl, etc.)..."
# No global brew installs needed!

echo "Cleaning build directory..."
rm -rf build-macos
mkdir -p build-macos

echo "Configuring CMake..."
# We use standard build, maybe bundle it later.
cmake -B build-macos -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DEBUG_API=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

echo "Building..."
cmake --build build-macos -j$(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: MacOS build finished!"
    echo "Binary: build-macos/hamclock-next"
    echo "--------------------------------------------------"
    
    # Optional: simple bundling
    # We could make a .app bundle here effectively manually or via CPack
    # For now, just the binary is a good start.
else
    echo "ERROR: Build failed!"
    exit 1
fi
