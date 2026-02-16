#!/bin/bash
# Raspberry Pi 3B (ARMv7 32-bit) Build for Bullseye (glibc 2.31)
# This build includes KMSDRM and GLES support for hardware acceleration on RPi.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"

# Clean build directory
echo "Cleaning old build artifacts..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE rm -rf build-rpi-bullseye-fb0
mkdir -p build-rpi-bullseye-fb0

# Run Build
echo "Starting Raspberry Pi Bullseye-FB0 Build (ARMv7)..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE bash -c "
    dpkg --add-architecture armhf && \
    apt-get update && apt-get install -y \
        build-essential cmake git gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf pkg-config ca-certificates \
        libatomic1-armhf-cross \
        libdrm-dev:armhf libgbm-dev:armhf libegl-dev:armhf libgles-dev:armhf \
        libdbus-1-dev:armhf libudev-dev:armhf libasound2-dev:armhf python3 && \
    export PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    cmake -Bbuild-rpi-bullseye-fb0 -H. \
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
        -DSDL_X11=OFF \
        -DSDL_WAYLAND=OFF \
        -DSDL_KMSDRM=ON \
        -DSDL_RPI=OFF \
        -DSDL_OPENGL=OFF \
        -DSDL_GLES=ON \
        -DSDL2IMAGE_VENDORED=ON \
        -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_WEBP=OFF \
        -DSDL2IMAGE_TIF=OFF \
        -DSDL2IMAGE_JXL=OFF \
        -DSDL2IMAGE_AVIF=OFF && \
    cmake --build build-rpi-bullseye-fb0 -j\$(nproc)
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Raspberry Pi Bullseye-FB0 build finished!"
    echo "Binary: build-rpi-bullseye-fb0/hamclock-next"

    # Create Debian Package
    echo "Packaging..."
    chmod +x packaging/linux/create_deb.sh
    ./packaging/linux/create_deb.sh "build-rpi-bullseye-fb0/hamclock-next" "armhf" "bullseye" "fb0" "build-rpi-bullseye-fb0"

    # Create release directory and copy artifacts
    echo "Copying artifacts to release/..."
    mkdir -p release
    VERSION="${VERSION:-0.5B}"

    # Copy .deb package
    cp "build-rpi-bullseye-fb0/hamclock-next_${VERSION}_bullseye_fb0_armhf.deb" "release/"

    # Copy binary with consistent naming
    cp "build-rpi-bullseye-fb0/hamclock-next" "release/hamclock-next-bullseye-fb0-armhf"

    echo "--------------------------------------------------"
    echo "Release artifacts:"
    echo "  - release/hamclock-next_${VERSION}_bullseye_fb0_armhf.deb"
    echo "  - release/hamclock-next-bullseye-fb0-armhf"
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
