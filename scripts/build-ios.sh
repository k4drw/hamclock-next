#!/bin/bash
# scripts/build-ios.sh
# Build HamClock-Next for iOS Simulator (arm64, unsigned).
# Requires: macOS + Xcode command-line tools.
# Not supported on Linux — Apple's iOS SDK is macOS-only.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-ios"

DEBUG_API_FLAG="-DENABLE_DEBUG_API=OFF"
for arg in "$@"; do
    if [ "$arg" == "--enable-debug-api" ]; then
        DEBUG_API_FLAG="-DENABLE_DEBUG_API=ON"
    fi
done

if [[ "$(uname)" != "Darwin" ]]; then
    echo "Error: iOS builds require macOS + Xcode. Linux is not supported."
    exit 1
fi

if ! command -v xcodebuild &>/dev/null; then
    echo "Error: xcodebuild not found. Install Xcode from the App Store."
    exit 1
fi

echo "==================================================="
echo "    HamClock-Next iOS Simulator Build Script       "
echo "==================================================="

echo "Cleaning previous build..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "[1/2] Configuring with CMake (Xcode generator, iOS)..."
cmake -B "$BUILD_DIR" \
    -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
    -DBUILD_TESTING=OFF \
    -DENABLE_TESTING=OFF \
    -DBUILD_CURL_EXE=OFF \
    ${DEBUG_API_FLAG} \
    "$REPO_ROOT"

# Discover the generated Xcode project name
XCODEPROJ=$(find "$BUILD_DIR" -maxdepth 1 -name "*.xcodeproj" | head -1)
if [[ -z "$XCODEPROJ" ]]; then
    echo "Error: No .xcodeproj found in $BUILD_DIR after configure."
    exit 1
fi
XCODEPROJ_NAME="$(basename "$XCODEPROJ")"
echo "Found Xcode project: $XCODEPROJ_NAME"

echo "[1.5/2] Patching Xcode project: stripping macOS-only frameworks..."
# ApplicationServices and CoreServices are not available in the iOS/Simulator SDK.
# Some FetchContent deps (SDL2_image, FreeType, HarfBuzz) might add them via
# hardcoded flags or find_library() leaking from host. Strip them all here.
find "$BUILD_DIR" -name "*.pbxproj" \
    -exec sed -i '' -E 's/(-framework +ApplicationServices|-Wl,-framework,ApplicationServices|-framework +CoreServices|-Wl,-framework,CoreServices)//g' {} \;

echo "[2/2] Building for iOS Simulator (arm64, unsigned)..."
xcodebuild \
    -project "$BUILD_DIR/$XCODEPROJ_NAME" \
    -scheme hamclock-next \
    -configuration Release \
    -sdk iphonesimulator \
    -arch arm64 \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGNING_REQUIRED=NO \
    build

APP_PATH="$BUILD_DIR/Release-iphonesimulator/hamclock-next.app"
echo ""
echo "=== iOS Simulator Build Complete ==="
echo "App bundle: $APP_PATH"
echo ""
echo "To run in the iOS Simulator:"
echo "  xcrun simctl install booted \"$APP_PATH\""
echo "  xcrun simctl launch booted com.k4drw.hcn"
