#!/bin/bash
# Direct Docker cross-compile for Windows x64 (Reliable Clean Build)

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$SCRIPT_DIR")
cd "$REPO_ROOT" || exit 1

IMAGE="dockcross/windows-static-x64"

# Clean build directory using the container to avoid permission issues
# TIP: Comment out the 'rm -rf' line below to keep _deps for faster subsequent builds
echo "Cleaning old build artifacts..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE rm -rf build-win64
mkdir -p build-win64

# Run CMake and Build with explicit override flags
echo "Starting Universal Windows Build..."
docker run --rm -v "$(pwd)":/work -w /work $IMAGE bash -c "
    apt-get update && apt-get install -y nsis imagemagick && \
    convert packaging/icon.png -define icon:auto-resize=256,128,64,48,32,16 packaging/windows/icon.ico && \
    cmake -Bbuild-win64 -H. \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_DEBUG_API=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL2IMAGE_WEBP=OFF \
        -DSDL2IMAGE_TIF=OFF \
        -DSDL2IMAGE_JXL=OFF \
        -DSDL2IMAGE_AVIF=OFF \
        -DSDL2IMAGE_SAMPLES=OFF \
        -DSDL2IMAGE_VENDORED=ON && \
    cmake --build build-win64 -j\$(nproc) && \
    makensis packaging/windows/installer.nsi && \
    chown -R $(id -u):$(id -g) build-win64 packaging/windows/icon.ico
"

if [ $? -eq 0 ]; then
    echo "--------------------------------------------------"
    echo "SUCCESS: Windows build finished!"
    echo "Binary: build-win64/hamclock-next.exe"
    echo "Installer: build-win64/HamClock-Next-Setup.exe"
    echo "--------------------------------------------------"
else
    echo "ERROR: Build failed!"
    exit 1
fi
