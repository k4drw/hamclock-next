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

# Generate .icns icon if it doesn't exist
if [ ! -f "assets/macos/HamClockNext.icns" ]; then
    echo "Generating macOS icon..."
    ./scripts/create-macos-icon.sh
fi

echo "Configuring CMake..."
# We use standard build, maybe bundle it later.
cmake -B build-macos -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_DEBUG_API=OFF \
    -DCMAKE_OSX_ARCHITECTURES="arm64"

echo "Building..."
cmake --build build-macos -j$(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: MacOS .app bundle created!"
    echo "Bundle: build-macos/hamclock-next.app"
    echo "--------------------------------------------------"

    # Create DMG for distribution
    echo "Creating DMG installer..."
    APP_NAME="HamClock-Next"
    ARCH=$(uname -m)
    DMG_NAME="hamclock-next-macos-${ARCH}.dmg"

    # Clean up any existing DMG
    rm -f "build-macos/$DMG_NAME"

    # Create temporary DMG staging directory
    mkdir -p "build-macos/dmg-staging"
    cp -R "build-macos/hamclock-next.app" "build-macos/dmg-staging/"

    # Create symlink to Applications folder for drag-and-drop install
    ln -s /Applications "build-macos/dmg-staging/Applications"

    # Create DMG using hdiutil
    hdiutil create -volname "$APP_NAME" \
        -srcfolder "build-macos/dmg-staging" \
        -ov -format UDZO \
        "build-macos/$DMG_NAME"

    # Clean up staging
    rm -rf "build-macos/dmg-staging"

    # Also create ZIP for alternative distribution
    echo "Creating ZIP archive..."
    cd build-macos
    zip -r "hamclock-next-macos-${ARCH}.zip" hamclock-next.app
    cd ..

    echo "--------------------------------------------------"
    echo "Distribution files created:"
    echo "  DMG: build-macos/$DMG_NAME"
    echo "  ZIP: build-macos/hamclock-next-macos-${ARCH}.zip"
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
