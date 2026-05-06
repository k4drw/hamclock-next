#!/bin/bash
# Generic Linux x86_64 DEB Build (Ubuntu 22.04 base)
# This script spins up an Ubuntu container to build the DEB package.
# Two separate binaries are built (unified and fb0) so each knows its own variant.

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"
BUILD_DIR="build-linux-x64"

DEBUG_API_FLAG="-DENABLE_DEBUG_API=OFF"
for arg in "$@"; do
    if [ "$arg" == "--enable-debug-api" ]; then
        DEBUG_API_FLAG="-DENABLE_DEBUG_API=ON"
    fi
done

# Get version from centralized files
V_NUM=$(cat VERSION | tr -d '[:space:]')
V_SUF=$(cat VERSION_SUFFIX | tr -d '[:space:]')
VERSION="${V_NUM}${V_SUF}"

# Clean build directory
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Common cmake flags (everything except HAMCLOCK_BUILD_VARIANT).
# Must be a single line — expanded inside docker bash -c "...", newlines would
# be treated as command separators, not line continuations.
COMMON_FLAGS="-DCMAKE_BUILD_TYPE=Release ${DEBUG_API_FLAG} -DCURL_DISABLE_INSTALL=ON -DSDL_STATIC=ON -DSDL_SHARED=OFF -DSDL_X11=ON -DSDL_X11_DYNAMIC=libX11.so.6 -DSDL_WAYLAND=ON -DSDL_WAYLAND_DYNAMIC=libwayland-client.so.0 -DSDL_KMSDRM=ON -DSDL_GLES=ON -DSDL2IMAGE_VENDORED=ON -DSDL2IMAGE_SAMPLES=OFF -DHAMCLOCK_INSTALL_TYPE=DEB"

echo "Starting Linux x64 DEB Build (v${VERSION}) — unified + fb0 variants..."

docker run --rm -v "$(pwd)":/work:z -w /work $IMAGE bash -c "
    export DEBIAN_FRONTEND=noninteractive && \
    apt-get update && apt-get install -y \
        build-essential cmake git pkg-config ca-certificates \
        libx11-dev libxext-dev libxcursor-dev libxi-dev \
        libxrandr-dev libxss-dev libxxf86vm-dev libxinerama-dev \
        libwayland-dev libxkbcommon-dev wayland-protocols \
        libdrm-dev libgbm-dev libegl-dev libgles-dev \
        libdbus-1-dev libudev-dev libasound2-dev flite1-dev curl && \
    echo '--- Building unified variant ---' && \
    cmake -B$BUILD_DIR/unified -H. $COMMON_FLAGS -DHAMCLOCK_BUILD_VARIANT=unified && \
    cmake --build $BUILD_DIR/unified -j\$(nproc) && \
    echo '--- Building fb0 variant ---' && \
    cmake -B$BUILD_DIR/fb0 -H. $COMMON_FLAGS -DHAMCLOCK_BUILD_VARIANT=fb0 && \
    cmake --build $BUILD_DIR/fb0 -j\$(nproc) && \
    chown -R $(id -u):$(id -g) $BUILD_DIR
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Linux x64 Build finished!"
    echo "Binaries: $BUILD_DIR/unified/hamclock-next  $BUILD_DIR/fb0/hamclock-next"

    chmod +x packaging/linux/create_deb.sh
    export VERSION="${VERSION}"

    echo "Packaging Unified (Desktop) DEB..."
    cp $BUILD_DIR/unified/hamclock-next $BUILD_DIR/hamclock-next.unified.${VERSION}.linux-x64
    ./packaging/linux/create_deb.sh "$BUILD_DIR/unified/hamclock-next" "amd64" "unified" "$BUILD_DIR"

    echo "Packaging Lean (Kiosk/Headless) DEB..."
    cp $BUILD_DIR/fb0/hamclock-next $BUILD_DIR/hamclock-next.fb0.${VERSION}.linux-x64
    ./packaging/linux/create_deb.sh "$BUILD_DIR/fb0/hamclock-next" "amd64" "fb0" "$BUILD_DIR"

    echo "--------------------------------------------------"

    # Cleanup intermediate artifacts
    echo "Cleaning up intermediate build artifacts (saving disk space)..."
    rm -rf "$BUILD_DIR/unified" "$BUILD_DIR/fb0"
else
    echo "ERROR: Build failed!"
    exit 1
fi
