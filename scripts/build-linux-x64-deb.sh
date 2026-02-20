#!/bin/bash
# Generic Linux x86_64 DEB Build (Ubuntu 22.04 base)
# This script spins up an Ubuntu container to build the DEB package.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"
BUILD_DIR="build-linux-x64"

# Get version from centralized files
V_NUM=$(cat VERSION | tr -d '[:space:]')
V_SUF=$(cat VERSION_SUFFIX | tr -d '[:space:]')
VERSION="${V_NUM}${V_SUF}"

# Clean build directory
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "Starting Linux x64 DEB Build (v${VERSION})..."

docker run --rm -v "$(pwd)":/work:z -w /work $IMAGE bash -c "
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && apt-get install -y \
        build-essential cmake git pkg-config ca-certificates \
        libx11-dev libxext-dev libxcursor-dev libxi-dev \
        libxrandr-dev libxss-dev libxxf86vm-dev libxinerama-dev \
        libwayland-dev libxkbcommon-dev wayland-protocols \
        libdrm-dev libgbm-dev libegl-dev libgles-dev \
        libdbus-1-dev libudev-dev libasound2-dev curl && \
    cmake -B$BUILD_DIR -H. \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_DEBUG_API=OFF \
        -DCURL_DISABLE_INSTALL=ON \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_X11=ON \
        -DSDL_X11_DYNAMIC=libX11.so.6 \
        -DSDL_WAYLAND=ON \
        -DSDL_WAYLAND_DYNAMIC=libwayland-client.so.0 \
        -DSDL_KMSDRM=ON \
        -DSDL_GLES=ON \
        -DSDL2IMAGE_VENDORED=ON \
        -DSDL2IMAGE_SAMPLES=OFF && \
    cmake --build $BUILD_DIR -j$(nproc) && \
    chown -R $(id -u):$(id -g) $BUILD_DIR
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Linux x64 Build finished!"
    echo "Binary: $BUILD_DIR/hamclock-next"

    # Packaging - We create two packages with the same binary but different dependencies
    echo "Packaging Unified (Desktop) DEB..."
    chmod +x packaging/linux/create_deb.sh
    
    # We export VERSION so create_deb.sh can use it
    export VERSION="${VERSION}"
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "amd64" "unified" "$BUILD_DIR"

    echo "Packaging Lean (Kiosk/Headless) DEB..."
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "amd64" "fb0" "$BUILD_DIR"
    
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
