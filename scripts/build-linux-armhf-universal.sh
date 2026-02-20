# Universal ARMhf Build (X11 + Wayland + KMSDRM)
# Target: ARMv7/ARMhf (RPi Zero 2W 32-bit, Older Pi, SBCs)
# Baseline: Debian Bullseye (glibc 2.31) for maximum compatibility

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"
BUILD_DIR="build-linux-armhf-universal"

# Clean build directory
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Run Build
echo "Starting Unified ARMhf Build (X11 + KMSDRM)..."
docker run --rm -v "$(pwd)":/work:z -w /work $IMAGE bash -c "
    dpkg --add-architecture armhf && \
    apt-get update && apt-get install -y \
        build-essential cmake git gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf pkg-config ca-certificates \
        libx11-dev:armhf libxext-dev:armhf libxcursor-dev:armhf libxi-dev:armhf \
        libxrandr-dev:armhf libxss-dev:armhf libxxf86vm-dev:armhf libxinerama-dev:armhf \
        libwayland-dev:armhf libxkbcommon-dev:armhf wayland-protocols \
        libdrm-dev:armhf libgbm-dev:armhf libegl-dev:armhf libgles-dev:armhf \
        libdbus-1-dev:armhf libudev-dev:armhf libasound2-dev:armhf && \
    export PKG_CONFIG_PATH=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig && \
    cmake -B$BUILD_DIR -H. \
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
        -DSDL_X11_DYNAMIC=libX11.so.6 \
        -DSDL_WAYLAND=ON \
        -DSDL_WAYLAND_DYNAMIC=libwayland-client.so.0 \
        -DSDL_KMSDRM=ON \
        -DSDL_OPENGL=OFF \
        -DSDL_GLES=ON \
        -DSDL2IMAGE_VENDORED=ON \
        -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_WEBP=OFF \
        -DSDL2IMAGE_TIF=OFF \
        -DSDL2IMAGE_JXL=OFF \
        -DSDL2IMAGE_AVIF=OFF && \
    cmake --build $BUILD_DIR -j\$(nproc) && \
    chown -R $(id -u):$(id -g) $BUILD_DIR
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Unified ARMhf Build finished!"
    echo "Binary: $BUILD_DIR/hamclock-next"

    # Packaging - We create two packages with the same binary but different dependencies
    echo "Packaging Unified (Desktop) DEB..."
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "armhf" "unified" "$BUILD_DIR"

    echo "Packaging Lean (Kiosk/Headless) DEB..."
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "armhf" "fb0" "$BUILD_DIR"
    
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
