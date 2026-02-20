#!/bin/bash
# Generic script to package HamClock-Next as a .deb
# Usage: ./create_deb.sh <binary_path> <arch> <variant> <build_dir>

BINARY_PATH=$1
ARCH=$2
VARIANT=$3  # e.g. "fb0" or "x11"
BUILD_DIR=$4

if [ -z "$BINARY_PATH" ] || [ -z "$ARCH" ] || [ -z "$VARIANT" ] || [ -z "$BUILD_DIR" ]; then
    echo "Usage: $0 <binary_path> <arch> <variant> <build_dir>"
    exit 1
fi

VERSION="${VERSION:-0.7B}"
PKG_NAME="hamclock-next-${VARIANT}"
PKG_DIR="${BUILD_DIR}/package"

# Clean up any previous package dir
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR"/DEBIAN
mkdir -p "$PKG_DIR"/usr/bin
mkdir -p "$PKG_DIR"/usr/share/applications
mkdir -p "$PKG_DIR"/usr/share/hamclock-next
mkdir -p "$PKG_DIR"/usr/share/icons/hicolor/256x256/apps

# 1. Install Binary
cp "$BINARY_PATH" "$PKG_DIR/usr/bin/hamclock-next"
chmod 755 "$PKG_DIR/usr/bin/hamclock-next"

# 1b. Install Icon
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
REPO_ROOT=$(dirname "$(dirname "$SCRIPT_DIR")")
cp "$REPO_ROOT/packaging/icon.png" "$PKG_DIR/usr/share/icons/hicolor/256x256/apps/hamclock-next.png"

# 2. Add Desktop entry (for X11 builds mainly, but useful for menu entry too)
cat > "$PKG_DIR/usr/share/applications/hamclock-next.desktop" <<EOF
[Desktop Entry]
Name=HamClock Next
Comment=Portable Amateur Radio Clock
Exec=/usr/bin/hamclock-next
Icon=hamclock-next
Terminal=false
Type=Application
Categories=Utility;HamRadio;
EOF

# 3. Create Control File
# Determine dependencies based on variant
DEPENDS="libc6, libcurl4, libstdc++6"
if [ "$VARIANT" == "unified" ] || [ "$VARIANT" == "universal" ]; then
    # Full desktop dependencies (X11 + Wayland + DRM)
    VARIANT="unified"
    DEPENDS="$DEPENDS, libx11-6, libxext6, libxcursor1, libxi6, libxrandr2, libxss1, libxxf86vm1, libxinerama1, libwayland-client0, libwayland-cursor0, libwayland-egl1, libxkbcommon0, libgl1, libdrm2, libgbm1, libegl1, libgles2"
else
    # Lean headless/kiosk dependencies (DRM/GBM only - no X11/Wayland)
    DEPENDS="$DEPENDS, libdrm2, libgbm1, libegl1, libgles2"
    VARIANT="fb0"
fi

cat > "$PKG_DIR/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: HamClock Community
Depends: ${DEPENDS}
Section: hamradio
Priority: optional
Description: HamClock Next - Portable Amateur Radio Clock
 A modern, feature-rich clock for amateur radio operators.
 Features contest calendars, propagation predictions, satellite tracking, and more.
 Built for Linux (${VARIANT}).
EOF

# 4. Build Package
# Simplified naming: no OS distribution in filename
FINAL_FILENAME="${BUILD_DIR}/hamclock-next_${VERSION}_${VARIANT}_${ARCH}.deb"
dpkg-deb --build "$PKG_DIR" "$FINAL_FILENAME"

echo "Package created: $FINAL_FILENAME"
