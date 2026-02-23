#!/bin/bash
# MacOS Build Script
# This script is intended to be run on macOS (either locally or in CI)

set -euo pipefail

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
if ! command -v codesign &> /dev/null; then
    echo "codesign not found. Please install Xcode Command Line Tools."
    exit 1
fi

# Signing and notarization settings
MACOS_SIGN_MODE="${MACOS_SIGN_MODE:-adhoc}"             # adhoc | developer-id | auto
MACOS_CODESIGN_IDENTITY="${MACOS_CODESIGN_IDENTITY:--}" # '-' means ad-hoc signature
MACOS_NOTARIZE="${MACOS_NOTARIZE:-0}"                   # 0 | 1
MACOS_NOTARY_KEYCHAIN_PROFILE="${MACOS_NOTARY_KEYCHAIN_PROFILE:-}"
MACOS_NOTARY_APPLE_ID="${MACOS_NOTARY_APPLE_ID:-}"
MACOS_NOTARY_TEAM_ID="${MACOS_NOTARY_TEAM_ID:-}"
MACOS_NOTARY_APP_PASSWORD="${MACOS_NOTARY_APP_PASSWORD:-}"

SIGN_IDENTITY="$MACOS_CODESIGN_IDENTITY"
USE_DEVELOPER_ID=0
case "$MACOS_SIGN_MODE" in
    adhoc)
        SIGN_IDENTITY="-"
        ;;
    developer-id)
        if [[ -z "$SIGN_IDENTITY" || "$SIGN_IDENTITY" == "-" ]]; then
            echo "ERROR: MACOS_SIGN_MODE=developer-id requires MACOS_CODESIGN_IDENTITY."
            exit 1
        fi
        USE_DEVELOPER_ID=1
        ;;
    auto)
        if [[ -n "$SIGN_IDENTITY" && "$SIGN_IDENTITY" != "-" ]]; then
            USE_DEVELOPER_ID=1
        else
            SIGN_IDENTITY="-"
        fi
        ;;
    *)
        echo "ERROR: Invalid MACOS_SIGN_MODE '$MACOS_SIGN_MODE'. Use adhoc, developer-id, or auto."
        exit 1
        ;;
esac

if [[ "$MACOS_NOTARIZE" != "0" && "$MACOS_NOTARIZE" != "1" ]]; then
    echo "ERROR: MACOS_NOTARIZE must be 0 or 1."
    exit 1
fi

if [[ "$MACOS_NOTARIZE" == "1" ]]; then
    if [[ "$USE_DEVELOPER_ID" -ne 1 ]]; then
        echo "ERROR: Notarization requires Developer ID signing."
        exit 1
    fi
    if ! command -v xcrun &> /dev/null; then
        echo "ERROR: xcrun is required for notarization."
        exit 1
    fi
    if [[ -z "$MACOS_NOTARY_KEYCHAIN_PROFILE" ]]; then
        if [[ -z "$MACOS_NOTARY_APPLE_ID" || -z "$MACOS_NOTARY_TEAM_ID" || -z "$MACOS_NOTARY_APP_PASSWORD" ]]; then
            echo "ERROR: Notarization requires MACOS_NOTARY_KEYCHAIN_PROFILE or all of:"
            echo "       MACOS_NOTARY_APPLE_ID, MACOS_NOTARY_TEAM_ID, MACOS_NOTARY_APP_PASSWORD."
            exit 1
        fi
    fi
fi

echo "Signing configuration:"
echo "  Mode: $MACOS_SIGN_MODE"
echo "  Effective Identity: $SIGN_IDENTITY"
echo "  Notarization enabled: $MACOS_NOTARIZE"

echo "Using CMake to fetch and build dependencies locally (SDL2, Curl, etc.)..."
# No global brew installs needed!

echo "Cleaning build directory..."
rm -rf build-macos
mkdir -p build-macos

# Generate .icns icon if it doesn't exist
if [ ! -f "packaging/macos/HamClockNext.icns" ]; then
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

echo "--------------------------------------------------"
echo "SUCCESS: MacOS .app bundle created!"
echo "Bundle: build-macos/hamclock-next.app"
echo "--------------------------------------------------"

APP_PATH="build-macos/hamclock-next.app"
ARCH=$(uname -m)
DMG_NAME="hamclock-next-macos-${ARCH}.dmg"
DMG_PATH="build-macos/$DMG_NAME"
ZIP_PATH="build-macos/hamclock-next-macos-${ARCH}.zip"

echo "Signing app bundle..."
APP_SIGN_FLAGS=(--force --deep --verbose --sign "$SIGN_IDENTITY")
if [[ "$USE_DEVELOPER_ID" -eq 1 ]]; then
    APP_SIGN_FLAGS+=(--timestamp --options runtime)
fi
codesign "${APP_SIGN_FLAGS[@]}" "$APP_PATH"
codesign --verify --deep --strict --verbose=2 "$APP_PATH"

echo "Creating DMG installer..."
APP_NAME="HamClock-Next"

# Clean up any existing DMG
rm -f "$DMG_PATH"

# Create temporary DMG staging directory
mkdir -p "build-macos/dmg-staging"
cp -R "$APP_PATH" "build-macos/dmg-staging/"

# Create symlink to Applications folder for drag-and-drop install
ln -s /Applications "build-macos/dmg-staging/Applications"

# Create DMG using hdiutil
hdiutil create -volname "$APP_NAME" \
    -srcfolder "build-macos/dmg-staging" \
    -ov -format UDZO \
    "$DMG_PATH"

# Clean up staging
rm -rf "build-macos/dmg-staging"

echo "Signing DMG..."
DMG_SIGN_FLAGS=(--force --verbose --sign "$SIGN_IDENTITY")
if [[ "$USE_DEVELOPER_ID" -eq 1 ]]; then
    DMG_SIGN_FLAGS+=(--timestamp)
fi
codesign "${DMG_SIGN_FLAGS[@]}" "$DMG_PATH"
codesign --verify --verbose=2 "$DMG_PATH"

if [[ "$MACOS_NOTARIZE" == "1" ]]; then
    echo "Submitting DMG for notarization..."
    if [[ -n "$MACOS_NOTARY_KEYCHAIN_PROFILE" ]]; then
        xcrun notarytool submit "$DMG_PATH" \
            --keychain-profile "$MACOS_NOTARY_KEYCHAIN_PROFILE" \
            --wait
    else
        xcrun notarytool submit "$DMG_PATH" \
            --apple-id "$MACOS_NOTARY_APPLE_ID" \
            --team-id "$MACOS_NOTARY_TEAM_ID" \
            --password "$MACOS_NOTARY_APP_PASSWORD" \
            --wait
    fi
    echo "Stapling notarization ticket..."
    xcrun stapler staple "$DMG_PATH"
    xcrun stapler validate "$DMG_PATH"
fi

# Also create ZIP for alternative distribution
echo "Creating ZIP archive..."
(
    cd build-macos
    rm -f "hamclock-next-macos-${ARCH}.zip"
    zip -r "hamclock-next-macos-${ARCH}.zip" hamclock-next.app
)

echo "--------------------------------------------------"
echo "Distribution files created:"
echo "  DMG: $DMG_PATH"
echo "  ZIP: $ZIP_PATH"
echo "--------------------------------------------------"
