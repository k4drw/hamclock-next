#!/bin/bash
# Orange Pi Zero 2W (AArch64) Build for Bookworm (fb0)
# This build uses a Debian Bookworm container to match the target environment.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bookworm"

# Clean build directory
echo "Cleaning old build artifacts..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE rm -rf build-opi-bookworm-fb0
mkdir -p build-opi-bookworm-fb0

# Run Build
echo "Starting Orange Pi Zero 2W Bookworm-FB0 Build (AArch64)..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE bash -c "
    dpkg --add-architecture arm64 && \
    apt-get update && apt-get install -y \
        build-essential cmake git gcc-aarch64-linux-gnu g++-aarch64-linux-gnu pkg-config ca-certificates \
        libdrm-dev:arm64 libgbm-dev:arm64 libegl-dev:arm64 libgles-dev:arm64 \
        libdbus-1-dev:arm64 libudev-dev:arm64 libasound2-dev:arm64 && \
    export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig && \
    export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig && \
    cmake -Bbuild-opi-bookworm-fb0 -H. \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_DEBUG_API=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DCURL_DISABLE_INSTALL=ON \
        -DSDL_STATIC=ON \
        -DSDL_SHARED=OFF \
        -DSDL_X11=OFF \
        -DSDL_WAYLAND=OFF \
        -DSDL_KMSDRM=ON \
        -DSDL_OPENGL=OFF \
        -DSDL_GLES=ON \
        -DSDL2IMAGE_VENDORED=ON \
        -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_WEBP=OFF \
        -DSDL2IMAGE_TIF=OFF \
        -DSDL2IMAGE_JXL=OFF \
        -DSDL2IMAGE_AVIF=OFF && \
    cmake --build build-opi-bookworm-fb0 -j\$(nproc)
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Orange Pi Zero 2W Bookworm-FB0 build finished!"
    echo "Binary: build-opi-bookworm-fb0/hamclock-next"
    
     # Create Debian Package
    echo "Packaging..."
    chmod +x packaging/linux/create_deb.sh
    ./packaging/linux/create_deb.sh "build-opi-bookworm-fb0/hamclock-next" "arm64" "bookworm" "fb0" "build-opi-bookworm-fb0"
    
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
