# Universal ARM64 Build (X11 + Wayland + KMSDRM)
# Target: AArch64 (RPi 4/5, Orange Pi, Generic ARM64)
# Baseline: Debian Bullseye (glibc 2.31) for maximum compatibility

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="debian:bullseye"
BUILD_DIR="build-linux-arm64-universal"

# Clean build directory
echo "Cleaning old build artifacts..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# Run Build
echo "Starting Unified ARM64 Build (X11 + KMSDRM)..."
docker run --rm -v "$(pwd)":/work:z -w /work $IMAGE bash -c "
    dpkg --add-architecture arm64 && \
    apt-get update && apt-get install -y \
        build-essential cmake git gcc-aarch64-linux-gnu g++-aarch64-linux-gnu pkg-config ca-certificates \
        libx11-dev:arm64 libxext-dev:arm64 libxcursor-dev:arm64 libxi-dev:arm64 \
        libxrandr-dev:arm64 libxss-dev:arm64 libxxf86vm-dev:arm64 libxinerama-dev:arm64 \
        libwayland-dev:arm64 libxkbcommon-dev:arm64 wayland-protocols \
        libdrm-dev:arm64 libgbm-dev:arm64 libegl-dev:arm64 libgles-dev:arm64 \
        libdbus-1-dev:arm64 libudev-dev:arm64 libasound2-dev:arm64 && \
    export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig && \
    export PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig && \
    cmake -B$BUILD_DIR -H. \
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
    cmake --build $BUILD_DIR -j$(nproc) && \
    chown -R $(id -u):$(id -g) $BUILD_DIR
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Unified ARM64 Build finished!"
    echo "Binary: $BUILD_DIR/hamclock-next"

    # Packaging - We create two packages with the same binary but different dependencies
    echo "Packaging Unified (Desktop) DEB..."
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "arm64" "unified" "$BUILD_DIR"

    echo "Packaging Lean (Kiosk/Headless) DEB..."
    ./packaging/linux/create_deb.sh "$BUILD_DIR/hamclock-next" "arm64" "fb0" "$BUILD_DIR"
    
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
