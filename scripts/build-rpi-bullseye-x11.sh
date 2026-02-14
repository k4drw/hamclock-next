#!/bin/bash
# Raspberry Pi (ARMv7) X11 Build for Bullseye
# Target: RPi 3/4 Running Raspberry Pi OS (Bullseye) Desktop

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"

# Clean build directory
echo "Cleaning old build artifacts..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE rm -rf build-rpi-bullseye-x11
mkdir -p build-rpi-bullseye-x11

# Run Build
echo "Starting Raspberry Pi Bullseye-X11 Build (ARMv7)..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE bash -c "
    dpkg --add-architecture armhf && \
    apt-get update && apt-get install -y \
        build-essential cmake git gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf pkg-config ca-certificates \
        libx11-dev:armhf libxext-dev:armhf libxcursor-dev:armhf libxi-dev:armhf \
        libxrandr-dev:armhf libxss-dev:armhf libxxf86vm-dev:armhf libxinerama-dev:armhf \
        libdrm-dev:armhf libgbm-dev:armhf libegl-dev:armhf libgles-dev:armhf \
        libdbus-1-dev:armhf libudev-dev:armhf libasound2-dev:armhf && \
    export PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    cmake -Bbuild-rpi-bullseye-x11 -H. \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=arm \
        -DCMAKE_C_COMPILER=arm-linux-gnueabihf-gcc \
        -DCMAKE_CXX_COMPILER=arm-linux-gnueabihf-g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_DEBUG_API=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DCURL_DISABLE_INSTALL=ON \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_X11=ON \
        -DSDL_WAYLAND=OFF \
        -DSDL_KMSDRM=OFF \
        -DSDL_OPENGL=OFF \
        -DSDL_GLES=ON \
        -DSDL2IMAGE_VENDORED=ON \
        -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_WEBP=OFF \
        -DSDL2IMAGE_TIF=OFF \
        -DSDL2IMAGE_JXL=OFF \
        -DSDL2IMAGE_AVIF=OFF && \
    cmake --build build-rpi-bullseye-x11 -j\$(nproc)
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Raspberry Pi Bullseye-X11 build finished!"
    echo "Binary: build-rpi-bullseye-x11/hamclock-next"

    # Create Debian Package
    echo "Packaging..."
    chmod +x packaging/linux/create_deb.sh
    ./packaging/linux/create_deb.sh "build-rpi-bullseye-x11/hamclock-next" "armhf" "bullseye" "x11" "build-rpi-bullseye-x11"
    
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
